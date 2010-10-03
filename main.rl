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
void Parse(void*, int, struct token);

%%{
	machine ancient;

	newline = '\n' @{curline++;};
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
	digit+ { emit_double(lemon, ts, te); };

	# floats
	digit+ '.' digit+ { emit_double(lemon, ts, te); };

	# punctuation stuff
	'+' { emit_symbol(lemon, PLUS); };
	'-' { emit_symbol(lemon, MINUS); };
	'*' { emit_symbol(lemon, TIMES); };
	'/' { emit_symbol(lemon, DIVIDE); };
	'<' { emit_symbol(lemon, LESS); };
	'(' { emit_symbol(lemon, LPAREN); };
	')' { emit_symbol(lemon, RPAREN); };
	';' { emit_symbol(lemon, SEMICOLON); };
	'=' { emit_symbol(lemon, EQUALS); };
	',' { emit_symbol(lemon, COMMA); };
	'{' { emit_symbol(lemon, LBRACE); };
	'}' { emit_symbol(lemon, RBRACE); };

	# keywords
	'if' { emit_symbol(lemon, IF); };
	'else' { emit_symbol(lemon, ELSE); };
	'for' { emit_symbol(lemon, FOR); };
	'func' { emit_symbol(lemon, FUNC); };
	'foreign' { emit_symbol(lemon, FOREIGN); };
	'var' { emit_symbol(lemon, VAR); };
	'return' { emit_symbol(lemon, RET); };

	alnum_u = alnum | '_';
	alpha_u = alpha | '_';

	alpha_u alnum_u* { emit_ident(lemon, ts, te-ts); };

	*|;
}%%

%% write data;

#define DEF_T(tt) struct token t; t.type = tt
static void emit_symbol(void *lemon, int tok)
{
	DEF_T(tok);
	Parse(lemon, tok, t);
}

static void emit_ident(void *lemon, char *beg, int len)
{
	DEF_T(IDENT);
	t.ident.beg = beg;
	t.ident.len = len;
	Parse(lemon, IDENT, t);
}

static void emit_double(void *lemon, char *ts, char *te)
{
	char tmpbuf[512];
	int len = te - ts;
	assert(len < sizeof(tmpbuf));
	strncpy(tmpbuf, ts, len);
	tmpbuf[len] = '\0';

	double num = strtod(tmpbuf, 0);
	DEF_T(DOUBLE);
	t.num = num;
	Parse(lemon, DOUBLE, t);
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
	int cs, act, curline = 1;
	char *ts, *te, *eof;

	LLVMModuleRef llmod = LLVMModuleCreateWithName("main");

	//LLVMLinkInJIT();
	LLVMInitializeNativeTarget();

/*
	LLVMExecutionEngineRef engine;
	char *error = 0;
	LLVMCreateJITCompilerForModule(&engine, llmod, 2, &error);
	if (error) {
		printf("%s\n", error);
		return 1;
	}

	LLVMPassManagerRef pass = LLVMCreatePassManager();
	LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), pass);
	LLVMAddConstantPropagationPass(pass);
	LLVMAddInstructionCombiningPass(pass);
	LLVMAddPromoteMemoryToRegisterPass(pass);
	LLVMAddGVNPass(pass);
	LLVMAddCFGSimplificationPass(pass);
*/

	// init lexer
	%% write init;

	// init parser
	void *lemon = ParseAlloc(malloc);

#if 1
	// trash loading
	char *buf = malloc(65536);
	int n = fread(buf, 1, 65536, stdin);
	if (n == 65536) {
		printf("source code limit failure\n");
		return 1;
	}
	printf("read: %d\n", n);
	buf[n] = '\0';
	
	char *p	= buf;
	char *pe = buf + n + 1;

	%% write exec;

	Parse(lemon, 0, (struct token){0,0});
	print_ast(SSS);
	struct codegen_context ctx = {0, llmod};
	codegen(&ctx, SSS);
		
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
	
		Parse(lemon, 0, (struct token){0, 0});
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

	ParseFree(lemon, free);

	return 0;
}
