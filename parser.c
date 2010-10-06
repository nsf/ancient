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
