%token_type { struct token }

%left LESS.
%left PLUS MINUS.
%left DIVIDE TIMES.

%include {
#include <stdio.h>
#include <assert.h>
#include "grammar.h"
#include "parser.h"

struct stmts *SSS;
}

%syntax_error {
	printf("Syntax error!\n");
}

program ::= stmts(A). { SSS = A; }

//-------------------------------------------------------------------------
// ident
//-------------------------------------------------------------------------
%type ident { struct expr* }
ident(A) ::= IDENT(B). { A = new_ident_expr(B.ident.beg, B.ident.len); }

//-------------------------------------------------------------------------
// stmts
//-------------------------------------------------------------------------
%type stmts { struct stmts* }
stmts(A) ::= stmt(B). { A = new_stmts(B); }
stmts(A) ::= stmts(B) stmt(C). { ARRAY_APPEND(B->v, C); A = B; }

//-------------------------------------------------------------------------
// stmt
//-------------------------------------------------------------------------
%type stmt { struct stmt* }
stmt(A) ::= expr(B) SEMICOLON. { A = new_expr_stmt(B); } // expr stmt
stmt(A) ::= ident(B) EQUALS expr(C) SEMICOLON. { A = new_assign_stmt(B, C); }
stmt(A) ::= VAR ident(B) EQUALS expr(C) SEMICOLON. { A = new_var_stmt(B, C); }
stmt(A) ::= VAR ident(B) SEMICOLON. { A = new_var_stmt(B, 0); }
stmt(A) ::= RET expr(B) SEMICOLON. { A = new_return_stmt(B); }
stmt(A) ::= RET SEMICOLON. { A = new_return_stmt(0); }

// just a helper for block-based statements (func, if/else, for)
%type block { struct stmt* }
block(A) ::= LBRACE stmts(B) RBRACE. { A = new_block_stmt(B); }

stmt(A) ::= block(B). { A = B; } // block stmt itself
stmt(A) ::= IF expr(B) block(C). { A = new_ifelse_stmt(B, C, 0); } // if alone
stmt(A) ::= IF expr(B) block(C) ELSE block(D). { A = new_ifelse_stmt(B, C, D); }
stmt(A) ::= FOR expr(COND) block(B).
{
	A = new_for_stmt(COND, B);
}
stmt(A) ::= FUNC ident(NAME) LPAREN args(ARGS) RPAREN block(B).
{
	A = new_func_stmt(NAME, ARGS, B);
}
stmt(A) ::= FUNC ident(NAME) block(B).
{
	A = new_func_stmt(NAME, 0, B);
}
stmt(A) ::= FOREIGN ident(NAME) LPAREN args(ARGS) RPAREN SEMICOLON.
{
	A = new_func_stmt(NAME, ARGS, 0);
}
stmt(A) ::= FOREIGN ident(NAME) SEMICOLON.
{
	A = new_func_stmt(NAME, 0, 0);
}

//-------------------------------------------------------------------------
// expr
//-------------------------------------------------------------------------
%type expr { struct expr* }
expr(A) ::= LPAREN expr(B) RPAREN. { A = B; }
expr(A) ::= expr(B) MINUS expr(C). { A = new_binary_expr(MINUS, B, C); }
expr(A) ::= expr(B) PLUS expr(C). { A = new_binary_expr(PLUS, B, C); }
expr(A) ::= expr(B) TIMES expr(C). { A = new_binary_expr(TIMES, B, C); }
expr(A) ::= expr(B) DIVIDE expr(C). { A = new_binary_expr(DIVIDE, B, C); }
expr(A) ::= expr(B) LESS expr(C). { A = new_binary_expr(LESS, B, C); }
expr(A) ::= DOUBLE(B). { A = new_num_expr(B.num); }
expr(A) ::= ident(B). { A = B; }
expr(A) ::= ident(B) LPAREN args(C) RPAREN. { A = new_call_expr(B, C); }

//-------------------------------------------------------------------------
// args (expr list, delimiter: ',')
//-------------------------------------------------------------------------
%type args { struct args* }
args(A) ::= expr(B). { A = new_args(B); }
args(A) ::= args(B) COMMA expr(C). { ARRAY_APPEND(B->v, C); A = B; }
