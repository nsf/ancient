// vim: filetype=ragel
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <readline/readline.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/Scalar.h>
#include "grammar.h"
#include "parser.h"

extern struct stmts *SSS;

// lemon parser definitions
void *ParseAlloc(void*(*)(size_t));
void ParseFree(void*, void(*)(void*));
void Parse(void*, int, struct token, struct parser_context*);

%%{
	machine ancient;

	newline = '\n' @{lemon.line++;};
	any_count_line = any | newline;

	c_comment := any_count_line* :>> '*/' @{fgoto main;};

	main := |*

	# skip spaces
	any_count_line - 0x21..0x7e;

	# skip C++ comments
	'//' [^\n]* newline;

	# skip C comments
	'/*' {fgoto c_comment;};

	# integers
	digit+ { emit_double(&lemon, ts, te); };

	# floats
	digit+ '.' digit+ { emit_double(&lemon, ts, te); };

	# punctuation stuff
	'+' { emit_symbol(&lemon, PLUS, ts); };
	'-' { emit_symbol(&lemon, MINUS, ts); };
	'*' { emit_symbol(&lemon, TIMES, ts); };
	'/' { emit_symbol(&lemon, DIVIDE, ts); };
	'<' { emit_symbol(&lemon, LESS, ts); };
	'(' { emit_symbol(&lemon, LPAREN, ts); };
	')' { emit_symbol(&lemon, RPAREN, ts); };
	';' { emit_symbol(&lemon, SEMICOLON, ts); };
	'=' { emit_symbol(&lemon, EQUALS, ts); };
	',' { emit_symbol(&lemon, COMMA, ts); };
	'{' { emit_symbol(&lemon, LBRACE, ts); };
	'}' { emit_symbol(&lemon, RBRACE, ts); };

	# keywords
	'if'      { emit_symbol(&lemon, IF, ts); };
	'else'    { emit_symbol(&lemon, ELSE, ts); };
	'for'     { emit_symbol(&lemon, FOR, ts); };
	'func'    { emit_symbol(&lemon, FUNC, ts); };
	'foreign' { emit_symbol(&lemon, FOREIGN, ts); };
	'var'     { emit_symbol(&lemon, VAR, ts); };
	'return'  { emit_symbol(&lemon, RET, ts); };

	alnum_u = alnum | '_';
	alpha_u = alpha | '_';

	alpha_u alnum_u* { emit_ident(&lemon, ts, te-ts); };

	*|;
}%%

%% write data;

#define DEF_T(tt) struct token t; t.type = tt
static void emit_symbol(struct parser_context *ctx, int tok, char *ts)
{
	DEF_T(tok);
	ctx->lasttoken = tok;
	ctx->ts = ts;
	Parse(ctx->lemon, tok, t, ctx);
}

static void emit_ident(struct parser_context *ctx, char *beg, int len)
{
	DEF_T(IDENT);
	t.ident.beg = beg;
	t.ident.len = len;
	ctx->lasttoken = IDENT;
	ctx->ts = beg;
	Parse(ctx->lemon, IDENT, t, ctx);
}

static void emit_double(struct parser_context *ctx, char *ts, char *te)
{
	char tmpbuf[512];
	int len = te - ts;
	assert(len < sizeof(tmpbuf));
	strncpy(tmpbuf, ts, len);
	tmpbuf[len] = '\0';

	double num = strtod(tmpbuf, 0);
	DEF_T(DOUBLE);
	t.num = num;
	ctx->lasttoken = DOUBLE;
	ctx->ts = ts;
	Parse(ctx->lemon, DOUBLE, t, ctx);
}

static void llvm_wipe_module(LLVMModuleRef m)
{
	LLVMValueRef cur = LLVMGetFirstFunction(m);
	if (!cur)
		return;
	for (;;) {
		LLVMValueRef next = LLVMGetNextFunction(cur);
		LLVMDeleteFunction(cur);
		if (!next)
			break;
		cur = next;
	}
}

int main(int argc, char **argv)
{
	int cs, act;
	char *ts, *te, *eof;

	LLVMInitializeNativeTarget();

	LLVMPassManagerRef pass = LLVMCreatePassManager();
	LLVMAddConstantPropagationPass(pass);
	LLVMAddInstructionCombiningPass(pass);
	LLVMAddPromoteMemoryToRegisterPass(pass);
	LLVMAddGVNPass(pass);
	LLVMAddCFGSimplificationPass(pass);

	// init lexer
	%% write init;

	// init parser
	struct parser_context lemon = {
		ParseAlloc(malloc),
		1,
		-1,
		0,
		0
	};

#if 1
	// trash loading
	char *buf = malloc(65536);
	int n = fread(buf, 1, 65536, stdin);
	if (n == 65536) {
		fprintf(stderr, "source code limit failure\n");
		return 1;
	}
	buf[n] = '\0';
	
	char *p	= buf;
	char *pe = buf + n + 1;
	lemon.buf = buf;

	%% write exec;

	Parse(lemon.lemon, 0, (struct token){0,0}, &lemon);
	print_ast(SSS);
	LLVMModuleRef llmod = codegen(SSS);
	LLVMRunPassManager(pass, llmod);
	LLVMDumpModule(llmod);
	LLVMWriteBitcodeToFile(llmod, "out.bc");
#else
	// prompt
	for (;;) {
		char *line = readline("> ");
		if (!line)
			break;
		if (!*line) {
			free(line);
			continue;
		}
		if (strcmp("exit", line) == 0) 
			break;
		add_history(line);

		char *p = line;
		char *pe = line + strlen(line) + 1;

		%% write exec;
	
		Parse(lemon.lemon, 0, (struct token){0, 0}, &lemon);
		struct codegen_context ctx = {0, llmod};
		codegen(&ctx, SSS);

		//LLVMRunPassManager(pass, llmod);

		printf("-----------------------------------------\n");
		LLVMDumpModule(llmod);
		printf("-----------------------------------------\n");

		LLVMVerifyModule(llmod, LLVMAbortProcessAction, &error);

		LLVMValueRef llmain = LLVMGetNamedFunction(llmod, "main");
		if (!llmain) {
			printf("No 'main' function!\n");
		} else {
			double (*FP)() = LLVMGetPointerToGlobal(engine, llmain);
			printf("Result: %f\n", (*FP)());
		}
	
		// reset
		llvm_wipe_module(llmod);

		free(line);
	}
#endif

	ParseFree(lemon.lemon, free);

	return 0;
}
