%code requires {
    #include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "driver.h"

extern int yylex(void);
void yyerror(const char *message);
%}

%start program

%union {
    char *str;
    ASTNode *node;
    ASTList *list;
}

%token CLASS ELSE FI IF IN INHERITS ISVOID LET LOOP POOL THEN WHILE CASE ESAC NEW OF NOT TRUE FALSE REPIT UNTIL
%token <str> TYPEID OBJECTID INT_CONST STR_CONST T_ERROR
%token ASSIGN DARROW LE POW

%right LET
%right ASSIGN
%right NOT
%nonassoc '<' LE '='
%left '+' '-'
%left '*' '/'
%right ISVOID
%right '~'
%left '@'
%left '.'
%right POW

%type <node> program class_def feature formal attr_init expr let_binding case_branch
%type <list> class_list feature_list formals formals_opt actuals actuals_opt block_exprs let_bindings case_branches
%type <str> inherits_opt

%%

program
    : class_list
      {
          parse_root = ast_program($1, curr_lineno);
          $$ = parse_root;
      }
    ;

class_list
    : class_def
      { $$ = ast_list_append(NULL, $1); }
    | class_list class_def
      { $$ = ast_list_append($1, $2); }
    ;

class_def
    : CLASS TYPEID inherits_opt '{' feature_list '}' ';'
      {
          $$ = ast_new(AST_CLASS, curr_lineno, $2, $3, NULL, NULL, NULL, $5);
      }
    | error ';'
      {
          yyerrok;
          $$ = ast_error("invalid class definition", curr_lineno);
      }
    ;

inherits_opt
    : /* empty */
      { $$ = "Object"; }
    | INHERITS TYPEID
      { $$ = $2; }
    ;

feature_list
    : /* empty */
      { $$ = NULL; }
    | feature_list feature ';'
      { $$ = ast_list_append($1, $2); }
    | feature_list error ';'
      {
          yyerrok;
          $$ = ast_list_append($1, ast_error("invalid feature", curr_lineno));
      }
    ;

feature
    : OBJECTID '(' formals_opt ')' ':' TYPEID '{' expr '}'
      {
          $$ = ast_new(AST_METHOD, curr_lineno, $1, $6, $8, NULL, NULL, $3);
      }
    | OBJECTID ':' TYPEID attr_init
      {
          $$ = ast_new(AST_ATTRIBUTE, curr_lineno, $1, $3, $4, NULL, NULL, NULL);
      }
    ;

formals_opt
    : /* empty */
      { $$ = NULL; }
    | formals
      { $$ = $1; }
    ;

formals
    : formal
      { $$ = ast_list_append(NULL, $1); }
    | formals ',' formal
      { $$ = ast_list_append($1, $3); }
    ;

formal
    : OBJECTID ':' TYPEID
      { $$ = ast_new(AST_FORMAL, curr_lineno, $1, $3, NULL, NULL, NULL, NULL); }
    ;

attr_init
    : /* empty */
      { $$ = NULL; }
    | ASSIGN expr
      { $$ = $2; }
    ;

actuals_opt
    : /* empty */
      { $$ = NULL; }
    | actuals
      { $$ = $1; }
    ;

actuals
    : expr
      { $$ = ast_list_append(NULL, $1); }
    | actuals ',' expr
      { $$ = ast_list_append($1, $3); }
    ;

block_exprs
    : expr ';'
      { $$ = ast_list_append(NULL, $1); }
    | block_exprs expr ';'
      { $$ = ast_list_append($1, $2); }
    | block_exprs error ';'
      {
          yyerrok;
          $$ = ast_list_append($1, ast_error("invalid block expression", curr_lineno));
      }
    ;

let_bindings
    : let_binding
      { $$ = ast_list_append(NULL, $1); }
    | let_bindings ',' let_binding
      { $$ = ast_list_append($1, $3); }
    | let_bindings ',' error
      {
          yyerrok;
          $$ = ast_list_append($1, ast_error("invalid let binding", curr_lineno));
      }
    ;

let_binding
    : OBJECTID ':' TYPEID attr_init
      { $$ = ast_new(AST_LET_BINDING, curr_lineno, $1, $3, $4, NULL, NULL, NULL); }
    ;

case_branches
    : case_branch
      { $$ = ast_list_append(NULL, $1); }
    | case_branches case_branch
      { $$ = ast_list_append($1, $2); }
    ;

case_branch
    : OBJECTID ':' TYPEID DARROW expr ';'
      { $$ = ast_new(AST_CASE_BRANCH, curr_lineno, $1, $3, $5, NULL, NULL, NULL); }
    ;

expr
    : OBJECTID ASSIGN expr
      { $$ = ast_new(AST_ASSIGN, curr_lineno, $1, NULL, $3, NULL, NULL, NULL); }
    | expr '.' OBJECTID '(' actuals_opt ')' %prec '.'
      { $$ = ast_new(AST_DISPATCH, curr_lineno, $3, NULL, $1, NULL, NULL, $5); }
    | expr '@' TYPEID '.' OBJECTID '(' actuals_opt ')' %prec '.'
      { $$ = ast_new(AST_STATIC_DISPATCH, curr_lineno, $5, $3, $1, NULL, NULL, $7); }
    | OBJECTID '(' actuals_opt ')'
      { $$ = ast_new(AST_SELF_DISPATCH, curr_lineno, $1, NULL, NULL, NULL, NULL, $3); }
    | IF expr THEN expr ELSE expr FI
      { $$ = ast_new(AST_IF, curr_lineno, NULL, NULL, $2, $4, $6, NULL); }
    | WHILE expr LOOP expr POOL
      { $$ = ast_new(AST_WHILE, curr_lineno, NULL, NULL, $2, $4, NULL, NULL); }
    | '{' block_exprs '}'
      { $$ = ast_new(AST_BLOCK, curr_lineno, NULL, NULL, NULL, NULL, NULL, $2); }
    | LET let_bindings IN expr %prec LET
      { $$ = ast_new(AST_LET, curr_lineno, NULL, NULL, $4, NULL, NULL, $2); }
    | CASE expr OF case_branches ESAC
      { $$ = ast_new(AST_CASE, curr_lineno, NULL, NULL, $2, NULL, NULL, $4); }
    | NEW TYPEID
      { $$ = ast_new(AST_NEW, curr_lineno, $2, NULL, NULL, NULL, NULL, NULL); }
    | ISVOID expr
      { $$ = ast_new(AST_ISVOID, curr_lineno, NULL, NULL, $2, NULL, NULL, NULL); }
    | '~' expr
      { $$ = ast_new(AST_NEGATE, curr_lineno, NULL, NULL, $2, NULL, NULL, NULL); }
    | NOT expr
      { $$ = ast_new(AST_NOT, curr_lineno, NULL, NULL, $2, NULL, NULL, NULL); }
    | expr '+' expr
      { $$ = ast_new(AST_PLUS, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr '-' expr
      { $$ = ast_new(AST_MINUS, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr '*' expr
      { $$ = ast_new(AST_MULTIPLY, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr '/' expr
      { $$ = ast_new(AST_DIVIDE, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr POW expr
      { $$ = ast_new(AST_POWER, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr '<' expr
      { $$ = ast_new(AST_LESS_THAN, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr LE expr
      { $$ = ast_new(AST_LESS_EQUAL, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | expr '=' expr
      { $$ = ast_new(AST_EQUAL, curr_lineno, NULL, NULL, $1, $3, NULL, NULL); }
    | '(' expr ')'
      { $$ = $2; }
    | OBJECTID
      { $$ = ast_new(AST_IDENTIFIER, curr_lineno, $1, NULL, NULL, NULL, NULL, NULL); }
    | INT_CONST
      { $$ = ast_new(AST_INTEGER, curr_lineno, $1, NULL, NULL, NULL, NULL, NULL); }
    | STR_CONST
      { $$ = ast_new(AST_STRING, curr_lineno, $1, NULL, NULL, NULL, NULL, NULL); }
    | TRUE
      { $$ = ast_new(AST_BOOLEAN, curr_lineno, "true", NULL, NULL, NULL, NULL, NULL); }
    | FALSE
      { $$ = ast_new(AST_BOOLEAN, curr_lineno, "false", NULL, NULL, NULL, NULL, NULL); }
    | REPIT expr UNTIL expr
      { $$ = ast_new(AST_REPIT_UNTIL, curr_lineno, NULL, NULL, $2, $4, NULL, NULL); }
    
    ;

%%

void yyerror(const char *message) {
    if (last_lex_error != NULL) {
        fprintf(stderr, "%s:%d: lexical error: %s\n",
                source_name, last_lex_error_line, last_lex_error);
        free(last_lex_error);
        last_lex_error = NULL;
    } else {
        fprintf(stderr, "%s:%d: syntax error: %s\n", source_name, curr_lineno, message);
    }
    ++parse_error_count;
}
