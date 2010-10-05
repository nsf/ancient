#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include "parser.h"
#include "grammar.h"

static char tokchars[] = {
	[PLUS] = '+',
	[MINUS] = '-',
	[DIVIDE] = '/',
	[TIMES] = '*',
};

static const char *toknames[] = {
	[LESS] = "<",
	[PLUS] = "+",
	[MINUS] = "-",
	[DIVIDE] = "/",
	[TIMES] = "*",
	[IDENT] = "ident",
	[SEMICOLON] = ";",
	[EQUALS] = "=",
	[VAR] = "var",
	[RET] = "return",
	[LBRACE] = "{",
	[RBRACE] = "}",
	[IF] = "if",
	[ELSE] = "else",
	[FOR] = "for",
	[FUNC] = "func",
	[LPAREN] = "(",
	[RPAREN] = ")",
	[FOREIGN] = "foreign",
	[DOUBLE] = "double",
	[COMMA] = ",",
};

const char *tokname(int token)
{
	return toknames[token];
}

void print_syntax_error(struct parser_context *ctx, const char *msg, ...)
{
	char *beg = ctx->buf;
	if (ctx->line != 1) {
		char *iter = ctx->ts;
		while (*iter != '\n')
			iter--;
		beg = iter+1;
	}
	char *end = strchr(beg, '\n');
	// print string with an error
	if (end)
		fwrite(beg, 1, end-beg+1, stderr);
	else
		fprintf(stderr, "%s\n", beg);
	// print error pointer
	int i;
	for (i = 0; i < ctx->ts - beg; i++) {
		if (isspace(beg[i]))
			fputc(beg[i], stderr);
		else
			fputc(' ', stderr);
	}
	fputs("\033[1;31m^\033[0m\n", stderr);
	// print error message
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	fputs("\n", stderr);
	va_end(args);
}

#define DEF_E(tt) struct expr *e = malloc(sizeof(struct expr)); e->type = tt
struct expr *new_num_expr(double num)
{
	DEF_E(EXPR_NUM);
	e->num = num;
	return e;
}

struct expr *new_binary_expr(int tok, struct expr *lhs, struct expr *rhs)
{
	DEF_E(EXPR_BIN);
	e->bin.tok = tok;
	e->bin.lhs = lhs;
	e->bin.rhs = rhs;
	return e;
}

struct expr *new_ident_expr(char *beg, int len)
{
	DEF_E(EXPR_IDENT);
	e->ident.beg = beg;
	e->ident.len = len;
	return e;
}

struct expr *new_call_expr(struct expr *ident, struct args *args)
{
	DEF_E(EXPR_CALL);
	e->call.ident = ident;
	e->call.args = args;
	return e;
}
#undef DEF_E

#define DEF_S(tt) struct stmt *s = malloc(sizeof(struct stmt)); s->type = tt
struct stmt *new_expr_stmt(struct expr *e)
{
	DEF_S(STMT_EXPR);
	s->expr = e;
	return s;
}

struct stmt *new_assign_stmt(struct expr *ident, struct expr *rhs)
{
	DEF_S(STMT_ASSIGN);
	s->assign.ident = ident;
	s->assign.rhs = rhs;
	return s;
}
struct stmt *new_block_stmt(struct stmts *block)
{
	DEF_S(STMT_BLOCK);
	s->block = block;
	return s;
}
struct stmt *new_ifelse_stmt(struct expr *cond, struct stmt *b1, struct stmt *b2)
{
	DEF_S(STMT_IFELSE);
	s->ifelse.cond = cond;
	s->ifelse.block = b1;
	s->ifelse.elseblock = b2;
	return s;
}

struct stmt *new_for_stmt(struct expr *cond, struct stmt *block)
{
	DEF_S(STMT_FOR);
	s->forloop.cond = cond;
	s->forloop.block = block;
	return s;
}

struct stmt *new_func_stmt(struct expr *ident, struct args *args, struct stmt *b)
{
	DEF_S(STMT_FUNC);
	s->func.ident = ident;
	s->func.args = args;
	s->func.block = b;
	return s;
}

struct stmt *new_var_stmt(struct expr *ident, struct expr *init)
{
	DEF_S(STMT_VAR);
	s->var.ident = ident;
	s->var.init = init;
	return s;
}

struct stmt *new_return_stmt(struct expr *e)
{
	DEF_S(STMT_RETURN);
	s->ret = e;
	return s;
}
#undef DEF_S

struct stmts *new_stmts(struct stmt *s)
{
	struct stmts *ss = malloc(sizeof(struct stmts));
	INIT_ARRAY(ss->v, 4);
	ARRAY_APPEND(ss->v, s);
	return ss;
}

struct args *new_args(struct expr *e)
{
	struct args *aa = malloc(sizeof(struct args));
	INIT_ARRAY(aa->v, 4);
	ARRAY_APPEND(aa->v, e);
	return aa;
}

//-------------------------------------------------------------------------
// AST printing
//-------------------------------------------------------------------------

static void print_indent(int indent)
{
	int i;
	for (i = 0; i < indent; i++)
		printf("  ");
}

static void print_call_args(int, struct args*);

static void print_expr_r(int indent, struct expr *e)
{
	print_indent(indent);
	switch (e->type) {
	case EXPR_NUM:
		printf("NUMBER: %f\n", e->num);
		break;
	case EXPR_BIN:
		printf("BINARY: %c\n", tokchars[e->bin.tok]);
		print_expr_r(indent+1, e->bin.lhs);
		print_expr_r(indent+1, e->bin.rhs);
		break;
	case EXPR_IDENT:
		printf("IDENT: ");
		fwrite(e->ident.beg, 1, e->ident.len, stdout);
		printf("\n");
		break;
	case EXPR_CALL:
		printf("CALL: ");
		fwrite(e->call.ident->ident.beg, 1, e->call.ident->ident.len, stdout);
		printf("\n");
		print_call_args(indent+1, e->call.args);
		break;
	}
}

static void print_call_args(int indent, struct args *a)
{
	int i;
	for (i = 0; i < a->v_n; i++) {
		struct expr *e = a->v[i];
		print_expr_r(indent, e);
	}
}

static void print_assign_stmt(int indent, struct stmt *s)
{
	print_indent(indent);
	printf("ASSIGN STMT\n");
	print_expr_r(indent+1, s->assign.ident);
	print_expr_r(indent+1, s->assign.rhs);
}

static void print_block_stmt(int indent, struct stmts*);

static void print_ifelse_stmt(int indent, struct stmt *s)
{
	print_indent(indent);
	printf("IF\n");
	print_expr_r(indent+1, s->ifelse.cond);
	print_block_stmt(indent+1, s->ifelse.block->block);
	if (s->ifelse.elseblock) {
		print_indent(indent);
		printf("ELSE\n");
		print_block_stmt(indent+1, s->ifelse.elseblock->block);
	}
}

static void print_stmt(int indent, struct stmt *s);

static void print_for_stmt(int indent, struct stmt *s)
{
	print_indent(indent);
	printf("FOR\n");
	print_expr_r(indent+1, s->forloop.cond);
	print_block_stmt(indent+1, s->forloop.block->block);
}

static void print_func_stmt(int indent, struct stmt *s)
{
	print_indent(indent);
	if (s->func.block)
		printf("FUNC\n");
	else
		printf("FOREIGN FUNC\n");
	print_expr_r(indent+1, s->func.ident);
	if (s->func.args)
		print_call_args(indent+1, s->func.args);
	if (s->func.block)
		print_block_stmt(indent+1, s->func.block->block);
}

static void print_var_stmt(int indent, struct stmt *s)
{
	print_indent(indent);
	printf("VAR\n");
	print_expr_r(indent+1, s->var.ident);
	if (s->var.init)
		print_expr_r(indent+1, s->var.init);
}

static void print_return_stmt(int indent, struct stmt *s)
{
	print_indent(indent);
	printf("RETURN\n");
	if (s->ret)
		print_expr_r(indent+1, s->ret);
}

static void print_stmt(int indent, struct stmt *s)
{
	switch (s->type) {
	case STMT_EXPR:
		print_expr_r(indent, s->expr);
		break;
	case STMT_ASSIGN:
		print_assign_stmt(indent, s);
		break;
	case STMT_BLOCK:
		print_block_stmt(indent, s->block);
		break;
	case STMT_IFELSE:
		print_ifelse_stmt(indent, s);
		break;
	case STMT_FOR:
		print_for_stmt(indent, s);
		break;
	case STMT_FUNC:
		print_func_stmt(indent, s);
		break;
	case STMT_VAR:
		print_var_stmt(indent, s);
		break;
	case STMT_RETURN:
		print_return_stmt(indent, s);
		break;
	}
}

static void print_block_stmt(int indent, struct stmts *block)
{
	print_indent(indent);
	printf("BLOCK\n");
	int i;
	for (i = 0; i < block->v_n; i++) {
		struct stmt *s = block->v[i];
		print_stmt(indent+1, s);
	}
}

void print_ast(struct stmts *top)
{
	print_block_stmt(0, top);
}

//-------------------------------------------------------------------------
// codegen
//-------------------------------------------------------------------------

struct scope *new_scope()
{
	struct scope *s = malloc(sizeof(struct scope));
	INIT_EMPTY_ARRAY(s->values);
	return s;
}

LLVMValueRef in_scope(struct scope *s, const char *name)
{
	int len = strlen(name);
	int i;
	for (i = 0; i < s->values_n; i++) {
		char *beg = s->values[i].ident->ident.beg;
		int len = s->values[i].ident->ident.len;
		if (memcmp(beg, name, len) == 0)
			return s->values[i].value;
	}
	return 0;
}

void add_to_scope(struct scope *s, struct expr *ident, LLVMValueRef value)
{
	struct value v = {ident, value};
	ARRAY_APPEND(s->values, v);
}

static void ident_to_name(char *buf, int size, struct expr *e)
{
	char *beg = e->ident.beg;
	int len = e->ident.len;
	assert(len < size);

	memcpy(buf, beg, len);
	buf[len] = '\0';
}

LLVMValueRef in_scope_ident(struct scope *s, struct expr *ident)
{
	char tmpbuf[512];
	ident_to_name(tmpbuf, sizeof(tmpbuf), ident);
	return in_scope(s, tmpbuf);
}

static LLVMValueRef errorv(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	fputs("\n", stderr);
	va_end(args);
	return 0;
}

static int get_num_args(LLVMValueRef func)
{
	func = LLVMIsAFunction(func);
	assert(func != 0);

	return LLVMCountParams(func);
}

static LLVMValueRef codegen_expr(struct codegen_context *ctx, struct expr *e)
{
	switch (e->type) {
	case EXPR_NUM:
	{
		return LLVMConstReal(LLVMDoubleType(), e->num);
	}
	case EXPR_IDENT:
	{
		LLVMValueRef v = in_scope_ident(ctx->scope, e);
		if (!v) {
			char tmpbuf[512];

			ident_to_name(tmpbuf, sizeof(tmpbuf), e);
			LLVMValueRef F = LLVMGetNamedFunction(ctx->module, tmpbuf);

			if (!F)
				return errorv("Cannot resolve function: %s", tmpbuf);

			return LLVMBuildCall(ctx->builder, F, 0, 0, "calltmp");
		}
		return LLVMBuildLoad(ctx->builder, v, "loadtmp");
	}
	case EXPR_BIN:
	{
		LLVMValueRef L = codegen_expr(ctx, e->bin.lhs);
		LLVMValueRef R = codegen_expr(ctx, e->bin.rhs);
		if (L == 0 || R == 0)
			return errorv("Failed to codegen lhs or rhs for binaryop");

		switch (e->bin.tok) {
		case PLUS: return LLVMBuildFAdd(ctx->builder, L, R, "addtmp");
		case MINUS: return LLVMBuildFSub(ctx->builder, L, R, "subtmp");
		case TIMES: return LLVMBuildFMul(ctx->builder, L, R, "multmp");
		case DIVIDE: return LLVMBuildFDiv(ctx->builder, L, R, "divtmp");
		case LESS:
			     L = LLVMBuildFCmp(ctx->builder, LLVMRealULT, L, R,
					       "cmptmp");
			     return LLVMBuildUIToFP(ctx->builder, L,
						    LLVMDoubleType(), "booltmp");
		}
	}
	case EXPR_CALL:
	{
		char tmpbuf[512];

		ident_to_name(tmpbuf, sizeof(tmpbuf), e->call.ident);
		LLVMValueRef F = LLVMGetNamedFunction(ctx->module, tmpbuf);

		if (!F)
			return errorv("Cannot resolve function: %s", tmpbuf);

		int numargs = get_num_args(F);
		if (e->call.args->v_n != numargs)
			return errorv("Invalid number of arguments for function call: %s", tmpbuf);

		assert(numargs <= 16);
		LLVMValueRef args[16];
		int i;
		for (i = 0; i < numargs; i++)
			args[i] = codegen_expr(ctx, e->call.args->v[i]);

		return LLVMBuildCall(ctx->builder, F, args, numargs, "calltmp");
	}
	default:
		return 0;
	}
}

static LLVMValueRef codegen_entry_alloca(LLVMValueRef F, const char *name)
{
	LLVMBuilderRef builder = LLVMCreateBuilder();
	LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(F);
	LLVMValueRef first = LLVMGetFirstInstruction(entry);
	if (!first)
		LLVMPositionBuilderAtEnd(builder, entry);
	else
		LLVMPositionBuilderBefore(builder, first);
	LLVMValueRef v = LLVMBuildAlloca(builder, LLVMDoubleType(), name);
	LLVMDisposeBuilder(builder);
	return v;
}

static void codegen_func(struct codegen_context *ctx, struct stmt *s)
{
	LLVMTypeRef types[16];
	int numargs = s->func.args ? s->func.args->v_n : 0;
	assert(numargs <= 16);

	char tmpbuf[512];
	ident_to_name(tmpbuf, sizeof(tmpbuf), s->func.ident);

	if (strcmp(tmpbuf, "main") == 0) {
		strcpy(tmpbuf, "_anc_main");
		tmpbuf[strlen("_anc_main")] = '\0';
	}

	int i;
	for (i = 0; i < numargs; i++)
		types[i] = LLVMDoubleType();

	LLVMTypeRef FT = LLVMFunctionType(LLVMDoubleType(),
					  types, numargs, 0);
	LLVMValueRef F = LLVMAddFunction(ctx->module, tmpbuf, FT);
	LLVMSetLinkage(F, LLVMExternalLinkage);
	if (s->func.block == 0) {
		// foreign function
		return;
	}

	// otherwise we have a full function definition

	// add default entry block
	LLVMBasicBlockRef entry = LLVMAppendBasicBlock(F, "entry");

	// create builder for this function
	LLVMBuilderRef llb = LLVMCreateBuilder();
	LLVMPositionBuilderAtEnd(llb, entry);

	// name args
	LLVMValueRef args[16];
	LLVMGetParams(F, args);
	for (i = 0; i < numargs; i++) {
		struct expr *ident = s->func.args->v[i];
		ident_to_name(tmpbuf, sizeof(tmpbuf), ident);
		LLVMSetValueName(args[i], tmpbuf);

		if (in_scope(ctx->scope, tmpbuf)) {
			errorv("Redeclaration of a variable: %s\n", tmpbuf);
		} else {
			LLVMValueRef store = codegen_entry_alloca(F, tmpbuf);
			LLVMBuildStore(llb, args[i], store);
			add_to_scope(ctx->scope, ident, store);
		}
	}

	LLVMBuilderRef savebuilder = ctx->builder;
	ctx->builder = llb;
	ctx->F = F;

	int terminated = codegen(ctx, s->func.block->block);
	if (!terminated)
		LLVMBuildRet(llb, LLVMConstReal(LLVMDoubleType(), 0));

	ctx->builder = savebuilder;
	ctx->F = 0;
	LLVMDisposeBuilder(llb);
}

static void codegen_var(struct codegen_context *ctx, struct stmt *s)
{
	char tmpbuf[512];
	ident_to_name(tmpbuf, sizeof(tmpbuf), s->var.ident);

	if (in_scope(ctx->scope, tmpbuf)) {
		errorv("Redeclaration of a variable: %s\n", tmpbuf);
	} else {
		LLVMValueRef store = LLVMBuildAlloca(ctx->builder, LLVMDoubleType(),
						     tmpbuf);
		if (s->var.init) {
			LLVMValueRef init = codegen_expr(ctx, s->var.init);
			LLVMBuildStore(ctx->builder, init, store);
		} else {
			LLVMBuildStore(ctx->builder,
				       LLVMConstReal(LLVMDoubleType(), 0),
				       store);
		}
		add_to_scope(ctx->scope, s->var.ident, store);
	}
}

static void codegen_assign(struct codegen_context *ctx, struct stmt *s)
{
	char tmpbuf[512];
	ident_to_name(tmpbuf, sizeof(tmpbuf), s->var.ident);

	LLVMValueRef store = in_scope(ctx->scope, tmpbuf);
	if (!store) {
		errorv("Cannot resolve variable: %s\n", tmpbuf);
	} else {
		LLVMValueRef rhs = codegen_expr(ctx, s->assign.rhs);
		if (!rhs) {
			errorv("Can't evaluate rhs expr in an assign stmt");
			return;
		}
		LLVMBuildStore(ctx->builder, rhs, store);
	}
}

static void codegen_ifelse(struct codegen_context *ctx, struct stmt *s)
{
	LLVMValueRef cond = codegen_expr(ctx, s->ifelse.cond);
	if (!cond) {
		errorv("Can't evaluate condition inside if statement");
		return;
	}

	LLVMBasicBlockRef iftrue = LLVMAppendBasicBlock(ctx->F, "iftrue");
	LLVMBasicBlockRef iffalse = s->ifelse.elseblock ?
			LLVMAppendBasicBlock(ctx->F, "iffalse") : 0;
	LLVMBasicBlockRef end = LLVMAppendBasicBlock(ctx->F, "ifend");
	if (!iffalse)
		iffalse = end;

	LLVMValueRef ifcond = LLVMBuildFCmp(ctx->builder, LLVMRealONE, cond,
					    LLVMConstReal(LLVMDoubleType(), 0),
					    "ifcond");
	LLVMBuildCondBr(ctx->builder, ifcond, iftrue, iffalse);

	// true
	LLVMPositionBuilderAtEnd(ctx->builder, iftrue);
	int terminated = codegen(ctx, s->ifelse.block->block);
	if (!terminated)
		LLVMBuildBr(ctx->builder, end);

	// false
	if (s->ifelse.elseblock) {
		LLVMPositionBuilderAtEnd(ctx->builder, iffalse);
		int terminated = codegen(ctx, s->ifelse.elseblock->block);
		if (!terminated)
			LLVMBuildBr(ctx->builder, end);
	}

	LLVMPositionBuilderAtEnd(ctx->builder, end);
}

static void codegen_return(struct codegen_context *ctx, struct stmt *s)
{
	if (s->ret) {
		LLVMValueRef v = codegen_expr(ctx, s->ret);
		LLVMBuildRet(ctx->builder, v);
	} else {
		LLVMBuildRet(ctx->builder, LLVMConstReal(LLVMDoubleType(), 0));
	}
}

static void codegen_forloop(struct codegen_context *ctx, struct stmt *s)
{

	LLVMBasicBlockRef loopdecide = LLVMAppendBasicBlock(ctx->F, "loopdecide");
	LLVMBasicBlockRef loop = LLVMAppendBasicBlock(ctx->F, "loop");
	LLVMBasicBlockRef end = LLVMAppendBasicBlock(ctx->F, "endloop");

	LLVMBuildBr(ctx->builder, loopdecide);

	// loopdecide
	LLVMPositionBuilderAtEnd(ctx->builder, loopdecide);
	LLVMValueRef cond = codegen_expr(ctx, s->ifelse.cond);
	if (!cond) {
		errorv("Can't evaluate condition inside for statement");
		return;
	}
	LLVMValueRef loopcond = LLVMBuildFCmp(ctx->builder, LLVMRealONE, cond,
					      LLVMConstReal(LLVMDoubleType(), 0),
					      "loopcond");
	LLVMBuildCondBr(ctx->builder, loopcond, loop, end);


	// loop
	LLVMPositionBuilderAtEnd(ctx->builder, loop);
	int terminated = codegen(ctx, s->forloop.block->block);
	if (!terminated)
		LLVMBuildBr(ctx->builder, loopdecide);

	// end
	LLVMPositionBuilderAtEnd(ctx->builder, end);
}

// returns whether it was terminated or not
int codegen(struct codegen_context *ctx, struct stmts *ss)
{
	int i;
	for (i = 0; i < ss->v_n; i++) {
		struct stmt *s = ss->v[i];
		switch (s->type) {
		case STMT_EXPR:
			codegen_expr(ctx, s->expr);
			break;
		case STMT_FUNC:
			ctx->scope = new_scope();
			codegen_func(ctx, s);
			break;
		case STMT_RETURN:
			codegen_return(ctx, s);
			return 1; // terminated
			break;
		case STMT_VAR:
			codegen_var(ctx, s);
			break;
		case STMT_ASSIGN:
			codegen_assign(ctx, s);
			break;
		case STMT_IFELSE:
			codegen_ifelse(ctx, s);
			break;
		case STMT_FOR:
			codegen_forloop(ctx, s);
			break;
		}
	}
	return 0;
}
