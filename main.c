#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semant.h"

#include "ast.h"
#include "driver.h"

extern int yyparse(void);
extern FILE *yyin;
extern void yyrestart(FILE *input_file);

int curr_lineno = 1;
const char *source_name = "<stdin>";
ASTNode *parse_root = NULL;
int parse_error_count = 0;
char *last_lex_error = NULL;
int last_lex_error_line = 1;

void set_lexical_error(char *message, int line) {
    last_lex_error = message;
    last_lex_error_line = line;
}

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s [-q] [source.cl]\n", program);
    fprintf(stderr, "  -q  Parse without printing the AST.\n");
}

int main(int argc, char **argv) {
    FILE *input = stdin;
    int show_ast = 1;
    int argument = 1;
    int parse_status;

    if (argument < argc && strcmp(argv[argument], "-q") == 0) {
        show_ast = 0;
        ++argument;
    }

    if (argument < argc) {
        source_name = argv[argument++];
        input = fopen(source_name, "rb");
        if (input == NULL) {
            perror(source_name);
            return EXIT_FAILURE;
        }
    }

    if (argument != argc) {
        print_usage(argv[0]);
        if (input != stdin) fclose(input);
        return EXIT_FAILURE;
    }

    curr_lineno = 1;
    yyrestart(input);
    parse_status = yyparse();

    if (input != stdin) fclose(input);

if (parse_status == 0 && parse_error_count == 0 && parse_root != NULL) {
    int semant_errors = semant_check(parse_root);
    if (semant_errors == 0) {
        if (show_ast) {
            ast_print(parse_root, stdout);
            semant_print_symbol_table(stdout);
        }
        ast_free(parse_root);
        parse_root = NULL;
        return EXIT_SUCCESS;
    }
}

ast_free(parse_root);
parse_root = NULL;
return EXIT_FAILURE;
}
