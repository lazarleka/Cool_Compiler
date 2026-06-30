#ifndef COOL_AST_H
#define COOL_AST_H

#include <stdio.h>

typedef enum {
    AST_PROGRAM,
    AST_CLASS,
    AST_METHOD,
    AST_ATTRIBUTE,
    AST_FORMAL,
    AST_ASSIGN,
    AST_DISPATCH,
    AST_STATIC_DISPATCH,
    AST_SELF_DISPATCH,
    AST_IF,
    AST_WHILE,
    AST_BLOCK,
    AST_LET,
    AST_LET_BINDING,
    AST_CASE,
    AST_CASE_BRANCH,
    AST_NEW,
    AST_ISVOID,
    AST_NEGATE,
    AST_NOT,
    AST_PLUS,
    AST_MINUS,
    AST_MULTIPLY,
    AST_DIVIDE,
    AST_POWER,
    AST_LESS_THAN,
    AST_LESS_EQUAL,
    AST_EQUAL,
    AST_IDENTIFIER,
    AST_INTEGER,
    AST_STRING,
    AST_BOOLEAN,
    AST_ERROR,
    AST_REPIT_UNTIL
} ASTKind;

typedef struct ASTNode ASTNode;
typedef struct ASTList ASTList;

struct ASTList {
    ASTNode *node;
    ASTList *next;
};

struct ASTNode {
    ASTKind kind;
    int line;
    char *text;
    char *text2;
    char *inferred_type;
    ASTNode *child1;
    ASTNode *child2;
    ASTNode *child3;
    ASTList *list;
};

char *ast_strdup(const char *text);
ASTNode *ast_new(ASTKind kind, int line, const char *text, const char *text2,
                 ASTNode *child1, ASTNode *child2, ASTNode *child3, ASTList *list);
ASTList *ast_list_append(ASTList *list, ASTNode *node);
ASTNode *ast_program(ASTList *classes, int line);
ASTNode *ast_error(const char *message, int line);
void ast_print(const ASTNode *node, FILE *out);
void ast_free(ASTNode *node);

#endif
