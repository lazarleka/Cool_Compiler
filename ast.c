#include "ast.h"

#include <stdlib.h>
#include <string.h>

typedef struct VarValue VarValue;
typedef struct VarSummary VarSummary;

struct VarValue {
    char *value;
    ASTKind kind;
    VarValue *next;
};

struct VarSummary {
    char *name;
    VarValue *values;
    VarSummary *next;
};

char *ast_strdup(const char *text) {
    size_t length;
    char *copy;

    if (text == NULL) return NULL;
    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, length + 1U);
    return copy;
}

ASTNode *ast_new(ASTKind kind, int line, const char *text, const char *text2,
                 ASTNode *child1, ASTNode *child2, ASTNode *child3, ASTList *list) {
    ASTNode *node = calloc(1U, sizeof(*node));
    if (node == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }

    node->kind = kind;
    node->line = line;
    node->text = ast_strdup(text);
    node->text2 = ast_strdup(text2);
    node->inferred_type = NULL;
    node->child1 = child1;
    node->child2 = child2;
    node->child3 = child3;
    node->list = list;
    return node;
}

ASTList *ast_list_append(ASTList *list, ASTNode *node) {
    ASTList *entry = calloc(1U, sizeof(*entry));
    ASTList *tail;

    if (entry == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    entry->node = node;

    if (list == NULL) return entry;

    tail = list;
    while (tail->next != NULL) tail = tail->next;
    tail->next = entry;
    return list;
}

ASTNode *ast_program(ASTList *classes, int line) {
    return ast_new(AST_PROGRAM, line, NULL, NULL, NULL, NULL, NULL, classes);
}

ASTNode *ast_error(const char *message, int line) {
    return ast_new(AST_ERROR, line, message, NULL, NULL, NULL, NULL, NULL);
}

static const char *kind_name(ASTKind kind) {
    switch (kind) {
        case AST_PROGRAM: return "Program";
        case AST_CLASS: return "Class";
        case AST_METHOD: return "Method";
        case AST_ATTRIBUTE: return "Attribute";
        case AST_FORMAL: return "Formal";
        case AST_ASSIGN: return "Assign";
        case AST_DISPATCH: return "Dispatch";
        case AST_STATIC_DISPATCH: return "StaticDispatch";
        case AST_SELF_DISPATCH: return "SelfDispatch";
        case AST_IF: return "If";
        case AST_WHILE: return "While";
        case AST_BLOCK: return "Block";
        case AST_LET: return "Let";
        case AST_REPIT_UNTIL: return "RepitUntil";
        case AST_LET_BINDING: return "LetBinding";
        case AST_CASE: return "Case";
        case AST_CASE_BRANCH: return "CaseBranch";
        case AST_NEW: return "New";
        case AST_ISVOID: return "Isvoid";
        case AST_NEGATE: return "Negate";
        case AST_NOT: return "Not";
        case AST_PLUS: return "Plus";
        case AST_MINUS: return "Minus";
        case AST_MULTIPLY: return "Multiply";
        case AST_DIVIDE: return "Divide";
        case AST_POWER: return "Power";
        case AST_LESS_THAN: return "LessThan";
        case AST_LESS_EQUAL: return "LessEqual";
        case AST_EQUAL: return "Equal";
        case AST_IDENTIFIER: return "Identifier";
        case AST_INTEGER: return "Integer";
        case AST_STRING: return "String";
        case AST_BOOLEAN: return "Boolean";
        case AST_ERROR: return "Error";
    }
    return "Unknown";
}

static void print_indent(FILE *out, int depth) {
    int index;
    for (index = 0; index < depth; ++index) fputs("  ", out);
}

static void print_escaped(FILE *out, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    fputc('"', out);
    while (*cursor != '\0') {
        switch (*cursor) {
            case '\\': fputs("\\\\", out); break;
            case '"': fputs("\\\"", out); break;
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            case '\b': fputs("\\b", out); break;
            case '\f': fputs("\\f", out); break;
            default:
                if (*cursor < 32U || *cursor > 126U) {
                    fprintf(out, "\\x%02X", *cursor);
                } else {
                    fputc(*cursor, out);
                }
        }
        ++cursor;
    }
    fputc('"', out);
}

static void ast_print_node(const ASTNode *node, FILE *out, int depth) {
    ASTList *entry;

    if (node == NULL) return;

    print_indent(out, depth);
    fprintf(out, "%s [line %d]", kind_name(node->kind), node->line);
    if (node->text != NULL) {
        fputs(" text=", out);
        print_escaped(out, node->text);
    }
    if (node->text2 != NULL) {
        fputs(" type=", out);
        print_escaped(out, node->text2);
    }
    fputc('\n', out);

    ast_print_node(node->child1, out, depth + 1);
    ast_print_node(node->child2, out, depth + 1);
    ast_print_node(node->child3, out, depth + 1);

    for (entry = node->list; entry != NULL; entry = entry->next) {
        ast_print_node(entry->node, out, depth + 1);
    }
}

static VarSummary *find_var_summary(VarSummary *list, const char *name) {
    while (list != NULL) {
        if (strcmp(list->name, name) == 0) return list;
        list = list->next;
    }
    return NULL;
}

static VarSummary *add_var_summary(VarSummary **list, const char *name) {
    VarSummary *summary = calloc(1U, sizeof(*summary));
    VarSummary **tail;

    if (summary == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }

    summary->name = ast_strdup(name);

    tail = list;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = summary;
    return summary;
}

static void add_var_value(VarSummary **list, const char *name, const ASTNode *value) {
    VarSummary *summary;
    VarValue *entry;
    VarValue **tail;

    if (name == NULL || value == NULL || value->text == NULL) return;
    if (value->kind != AST_INTEGER && value->kind != AST_STRING && value->kind != AST_BOOLEAN) return;

    summary = find_var_summary(*list, name);
    if (summary == NULL) summary = add_var_summary(list, name);

    entry = calloc(1U, sizeof(*entry));
    if (entry == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }

    entry->value = ast_strdup(value->text);
    entry->kind = value->kind;

    tail = &summary->values;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = entry;
}

static void collect_var_values(const ASTNode *node, VarSummary **summaries) {
    ASTList *entry;

    if (node == NULL) return;

    if (node->kind == AST_ATTRIBUTE || node->kind == AST_LET_BINDING || node->kind == AST_ASSIGN) {
        add_var_value(summaries, node->text, node->child1);
    }

    collect_var_values(node->child1, summaries);
    collect_var_values(node->child2, summaries);
    collect_var_values(node->child3, summaries);

    for (entry = node->list; entry != NULL; entry = entry->next) {
        collect_var_values(entry->node, summaries);
    }
}

static void print_value(const VarValue *value, FILE *out) {
    if (value->kind == AST_STRING) {
        print_escaped(out, value->value);
    } else {
        fputs(value->value, out);
    }
}

static void print_var_summaries(const VarSummary *summaries, FILE *out) {
    const VarSummary *summary;
    const VarValue *value;
    int first;

    fputs("\nVariableValues\n", out);
    if (summaries == NULL) {
        fputs("  <none>\n", out);
        return;
    }

    for (summary = summaries; summary != NULL; summary = summary->next) {
        fprintf(out, "  %s: ", summary->name);
        first = 1;
        for (value = summary->values; value != NULL; value = value->next) {
            if (!first) fputs(", ", out);
            print_value(value, out);
            first = 0;
        }
        fputc('\n', out);
    }
}

static void free_var_summaries(VarSummary *summaries) {
    VarSummary *next_summary;
    VarValue *value;
    VarValue *next_value;

    while (summaries != NULL) {
        next_summary = summaries->next;
        value = summaries->values;
        while (value != NULL) {
            next_value = value->next;
            free(value->value);
            free(value);
            value = next_value;
        }
        free(summaries->name);
        free(summaries);
        summaries = next_summary;
    }
}

void ast_print(const ASTNode *node, FILE *out) {
    ast_print_node(node, out, 0);
    if (0) {
        VarSummary *summaries = NULL;
        collect_var_values(node, &summaries);
        print_var_summaries(summaries, out);
        free_var_summaries(summaries);
    }
}

static void ast_list_free(ASTList *list) {
    ASTList *next;
    while (list != NULL) {
        next = list->next;
        ast_free(list->node);
        free(list);
        list = next;
    }
}

void ast_free(ASTNode *node) {
    if (node == NULL) return;
    ast_free(node->child1);
    ast_free(node->child2);
    ast_free(node->child3);
    ast_list_free(node->list);
    free(node->text);
    free(node->text2);
    free(node->inferred_type);
    free(node);
}
