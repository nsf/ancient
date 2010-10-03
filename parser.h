#pragma once

#include <llvm-c/Core.h>
#include "array.h"

struct token {
	int type; // for types see grammar.h, it is generated by lemon
	union {
		double num;
		struct {
			char *beg;
			int len;
		} ident;
	};
};

struct expr;

struct args {
	DECLARE_ARRAY(struct expr*, v);
};

enum expr_type {
	EXPR_NUM,
	EXPR_BIN,
	EXPR_IDENT,
	EXPR_CALL,
};

struct expr {
	enum expr_type type;
	union {
		double num;
		struct {
			int tok;
			struct expr *lhs;
			struct expr *rhs;
		} bin;
		struct {
			char *beg;
			int len;
		} ident;
		struct {
			struct expr *ident;
			struct args *args;
		} call;
	};
};

enum stmt_type {
	STMT_EXPR,
	STMT_ASSIGN,
	STMT_BLOCK,
	STMT_IFELSE,
	STMT_FOR,
	STMT_FUNC,
	STMT_VAR,
	STMT_RETURN,
};

struct stmts;

struct stmt {
	enum stmt_type type;
	union {
		struct expr *expr;
		struct {
			struct expr *ident;
			struct expr *rhs;
		} assign;
		struct stmts *block;
		struct {
			struct expr *cond;
			struct stmt *block;
			struct stmt *elseblock; // optional
		} ifelse;
		struct {
			struct expr *cond;
			struct stmt *block;
		} forloop;
		struct {
			struct expr *ident;
			struct args *args; // optional

			// by convention if there are no block, this AST node
			// means foreign function declaration
			struct stmt *block;
		} func;
		struct {
			struct expr *ident;
			struct expr *init; // optional
		} var;
		struct expr *ret; // optional
	};
};

struct stmts {
	DECLARE_ARRAY(struct stmt*, v);
};

struct expr *new_num_expr(double num);
struct expr *new_binary_expr(int tok, struct expr *lhs, struct expr *rhs);
struct expr *new_ident_expr(char *beg, int len);
struct expr *new_call_expr(struct expr *ident, struct args *args);

struct stmt *new_expr_stmt(struct expr *e);
struct stmt *new_assign_stmt(struct expr *ident, struct expr *rhs);
struct stmt *new_block_stmt(struct stmts *block);
struct stmt *new_ifelse_stmt(struct expr *cond, struct stmt *b1, struct stmt *b2);
struct stmt *new_for_stmt(struct expr *cond, struct stmt *block);
struct stmt *new_func_stmt(struct expr *ident, struct args *args, struct stmt *b);
struct stmt *new_var_stmt(struct expr *ident, struct expr *init);
struct stmt *new_return_stmt(struct expr *e);

struct stmts *new_stmts(struct stmt *s);
struct args *new_args(struct expr *e);

void print_ast(struct stmts *top);

struct value {
	struct expr *ident;
	LLVMValueRef value;
};

struct scope {
	DECLARE_ARRAY(struct value, values);
};

struct scope *new_scope();
LLVMValueRef in_scope(struct scope *s, const char *name);
LLVMValueRef in_scope_ident(struct scope *s, struct expr *ident);
void add_to_scope(struct scope *s, struct expr *ident, LLVMValueRef value);

struct codegen_context {
	LLVMBuilderRef builder;
	LLVMModuleRef module;
	struct scope *scope;
	LLVMValueRef F;
};

int codegen(struct codegen_context *ctx, struct stmts *ss);