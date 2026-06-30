#ifndef COOL_SEMANT_H
#define COOL_SEMANT_H

#include "ast.h"
#include "driver.h"

int semant_check(ASTNode *program);
void semant_print_symbol_table(FILE *out);

#endif
