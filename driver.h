#ifndef COOL_DRIVER_H
#define COOL_DRIVER_H

#include "ast.h"

extern int curr_lineno;
extern const char *source_name;
extern ASTNode *parse_root;
extern int parse_error_count;
extern char *last_lex_error;
extern int last_lex_error_line;

void set_lexical_error(char *message, int line);

#endif
