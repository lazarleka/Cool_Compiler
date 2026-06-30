#include "semant.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * This semantic analyzer is tailored to the AST produced by this project:
 *   AST_CLASS      text=name,      text2=parent
 *   AST_METHOD     text=name,      text2=return_type, child1=body, list=formals
 *   AST_ATTRIBUTE  text=name,      text2=declared_type, child1=initializer
 *   AST_FORMAL     text=name,      text2=declared_type
 *   AST_LET_BINDING text=name,     text2=declared_type, child1=initializer
 *   AST_CASE_BRANCH text=name,     text2=declared_type, child1=body
 */

#define SELF_TYPE_NAME "SELF_TYPE"
#define OBJECT_TYPE    "Object"
#define IO_TYPE        "IO"
#define INT_TYPE       "Int"
#define STRING_TYPE    "String"
#define BOOL_TYPE      "Bool"
#define MAIN_CLASS     "Main"
#define MAIN_METHOD    "main"

typedef struct FormalInfo FormalInfo;
typedef struct MethodInfo MethodInfo;
typedef struct AttrInfo AttrInfo;
typedef struct ClassInfo ClassInfo;
typedef struct Scope Scope;
typedef struct VarBinding VarBinding;
typedef struct StringSet StringSet;

struct FormalInfo {
    char *name;
    char *type;
    FormalInfo *next;
};

struct MethodInfo {
    char *name;
    char *return_type;
    FormalInfo *formals;
    int formal_count;
    ASTNode *node;
    int builtin;
    MethodInfo *next;
};

struct AttrInfo {
    char *name;
    char *type;
    ASTNode *node;
    int builtin;
    AttrInfo *next;
};

struct ClassInfo {
    char *name;
    char *parent_name;
    ASTNode *node;
    int builtin;

    /* Feature declarations written directly in this class. */
    MethodInfo *methods;
    AttrInfo *attrs;

    /* Traversal/status flags. */
    int inheritance_state; /* 0=unvisited, 1=visiting, 2=done */
    int feature_state;     /* 0=unvisited, 1=visiting, 2=done */

    ClassInfo *next;
};

struct VarBinding {
    char *name;
    char *type;
    VarBinding *next;
};

struct Scope {
    VarBinding *bindings;
    Scope *next;
};

struct StringSet {
    char *value;
    StringSet *next;
};

static ClassInfo *g_classes = NULL;
static Scope *g_scope_stack = NULL;
static int g_semant_errors = 0;

/* ------------------------------------------------------------------------- */
/* Basic helpers                                                              */
/* ------------------------------------------------------------------------- */

static void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (ptr == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static char *xstrdup(const char *text) {
    char *copy;
    size_t n;

    if (text == NULL) return NULL;
    n = strlen(text);
    copy = (char *)malloc(n + 1U);
    if (copy == NULL) {
        fprintf(stderr, "Fatal error: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, n + 1U);
    return copy;
}

static int streq(const char *a, const char *b) {
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int is_self_type_name(const char *type_name) {
    return streq(type_name, SELF_TYPE_NAME);
}

static int is_basic_type_name(const char *type_name) {
    return streq(type_name, INT_TYPE) ||
           streq(type_name, STRING_TYPE) ||
           streq(type_name, BOOL_TYPE);
}

static void set_inferred_type(ASTNode *node, const char *type_name) {
    if (node == NULL) return;
    free(node->inferred_type);
    node->inferred_type = xstrdup(type_name != NULL ? type_name : OBJECT_TYPE);
}

void semant_error(ASTNode *node, const char *fmt, ...) {
    va_list ap;
    int line = 0;

    if (node != NULL) line = node->line;

    fprintf(stderr, "%s:%d: semantic error: ",
            source_name != NULL ? source_name : "<stdin>", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    ++g_semant_errors;
    ++parse_error_count;
}

/* ------------------------------------------------------------------------- */
/* Class table                                                                */
/* ------------------------------------------------------------------------- */

static ClassInfo *find_class(const char *name) {
    ClassInfo *cls;
    for (cls = g_classes; cls != NULL; cls = cls->next) {
        if (streq(cls->name, name)) return cls;
    }
    return NULL;
}

static int class_exists(const char *name) {
    return find_class(name) != NULL;
}

static int is_valid_type(const char *type_name) {
    if (type_name == NULL) return 0;
    if (is_self_type_name(type_name)) return 1;
    return class_exists(type_name);
}

static ClassInfo *new_class(const char *name, const char *parent, ASTNode *node, int builtin) {
    ClassInfo *cls = (ClassInfo *)xcalloc(1U, sizeof(*cls));
    cls->name = xstrdup(name);
    cls->parent_name = xstrdup(parent);
    cls->node = node;
    cls->builtin = builtin;
    cls->next = g_classes;
    g_classes = cls;
    return cls;
}

static FormalInfo *new_formal(const char *name, const char *type) {
    FormalInfo *formal = (FormalInfo *)xcalloc(1U, sizeof(*formal));
    formal->name = xstrdup(name);
    formal->type = xstrdup(type);
    return formal;
}

static MethodInfo *new_method(const char *name, const char *return_type,
                              FormalInfo *formals, int formal_count,
                              ASTNode *node, int builtin) {
    MethodInfo *method = (MethodInfo *)xcalloc(1U, sizeof(*method));
    method->name = xstrdup(name);
    method->return_type = xstrdup(return_type);
    method->formals = formals;
    method->formal_count = formal_count;
    method->node = node;
    method->builtin = builtin;
    return method;
}

static AttrInfo *new_attr(const char *name, const char *type, ASTNode *node, int builtin) {
    AttrInfo *attr = (AttrInfo *)xcalloc(1U, sizeof(*attr));
    attr->name = xstrdup(name);
    attr->type = xstrdup(type);
    attr->node = node;
    attr->builtin = builtin;
    return attr;
}

static void class_add_method(ClassInfo *cls, MethodInfo *method) {
    MethodInfo **tail = &cls->methods;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = method;
}

static void class_add_attr(ClassInfo *cls, AttrInfo *attr) {
    AttrInfo **tail = &cls->attrs;
    while (*tail != NULL) tail = &(*tail)->next;
    *tail = attr;
}

static FormalInfo *formal_append(FormalInfo *list, FormalInfo *node) {
    FormalInfo *tail;
    if (list == NULL) return node;
    tail = list;
    while (tail->next != NULL) tail = tail->next;
    tail->next = node;
    return list;
}

static MethodInfo *find_method_in_class_only(const ClassInfo *cls, const char *name) {
    MethodInfo *m;
    if (cls == NULL) return NULL;
    for (m = cls->methods; m != NULL; m = m->next) {
        if (streq(m->name, name)) return m;
    }
    return NULL;
}

static AttrInfo *find_attr_in_class_only(const ClassInfo *cls, const char *name) {
    AttrInfo *a;
    if (cls == NULL) return NULL;
    for (a = cls->attrs; a != NULL; a = a->next) {
        if (streq(a->name, name)) return a;
    }
    return NULL;
}

static MethodInfo *find_method_in_ancestors(const ClassInfo *cls, const char *name) {
    ClassInfo *parent;
    if (cls == NULL || cls->parent_name == NULL) return NULL;
    parent = find_class(cls->parent_name);
    while (parent != NULL) {
        MethodInfo *m = find_method_in_class_only(parent, name);
        if (m != NULL) return m;
        parent = parent->parent_name != NULL ? find_class(parent->parent_name) : NULL;
    }
    return NULL;
}

static AttrInfo *find_attr_in_ancestors(const ClassInfo *cls, const char *name) {
    ClassInfo *parent;
    if (cls == NULL || cls->parent_name == NULL) return NULL;
    parent = find_class(cls->parent_name);
    while (parent != NULL) {
        AttrInfo *a = find_attr_in_class_only(parent, name);
        if (a != NULL) return a;
        parent = parent->parent_name != NULL ? find_class(parent->parent_name) : NULL;
    }
    return NULL;
}

static MethodInfo *lookup_method(const ClassInfo *cls, const char *name) {
    const ClassInfo *cursor = cls;
    while (cursor != NULL) {
        MethodInfo *m = find_method_in_class_only(cursor, name);
        if (m != NULL) return m;
        cursor = cursor->parent_name != NULL ? find_class(cursor->parent_name) : NULL;
    }
    return NULL;
}

static void builtin_method0(ClassInfo *cls, const char *name, const char *ret) {
    class_add_method(cls, new_method(name, ret, NULL, 0, NULL, 1));
}

static void builtin_method1(ClassInfo *cls, const char *name,
                            const char *arg1_name, const char *arg1_type,
                            const char *ret) {
    FormalInfo *f = new_formal(arg1_name, arg1_type);
    class_add_method(cls, new_method(name, ret, f, 1, NULL, 1));
}

static void builtin_method2(ClassInfo *cls, const char *name,
                            const char *arg1_name, const char *arg1_type,
                            const char *arg2_name, const char *arg2_type,
                            const char *ret) {
    FormalInfo *f = NULL;
    f = formal_append(f, new_formal(arg1_name, arg1_type));
    f = formal_append(f, new_formal(arg2_name, arg2_type));
    class_add_method(cls, new_method(name, ret, f, 2, NULL, 1));
}

void add_builtin_classes(void) {
    ClassInfo *object_cls;
    ClassInfo *io_cls;
    ClassInfo *int_cls;
    ClassInfo *string_cls;
    ClassInfo *bool_cls;

    object_cls = new_class(OBJECT_TYPE, NULL, NULL, 1);
    builtin_method0(object_cls, "abort", OBJECT_TYPE);
    builtin_method0(object_cls, "type_name", STRING_TYPE);
    builtin_method0(object_cls, "copy", SELF_TYPE_NAME);

    io_cls = new_class(IO_TYPE, OBJECT_TYPE, NULL, 1);
    builtin_method1(io_cls, "out_string", "x", STRING_TYPE, SELF_TYPE_NAME);
    builtin_method1(io_cls, "out_int", "x", INT_TYPE, SELF_TYPE_NAME);
    builtin_method0(io_cls, "in_string", STRING_TYPE);
    builtin_method0(io_cls, "in_int", INT_TYPE);

    int_cls = new_class(INT_TYPE, OBJECT_TYPE, NULL, 1);
    string_cls = new_class(STRING_TYPE, OBJECT_TYPE, NULL, 1);
    builtin_method0(string_cls, "length", INT_TYPE);
    builtin_method1(string_cls, "concat", "s", STRING_TYPE, STRING_TYPE);
    builtin_method2(string_cls, "substr", "i", INT_TYPE, "l", INT_TYPE, STRING_TYPE);

    bool_cls = new_class(BOOL_TYPE, OBJECT_TYPE, NULL, 1);
    (void)int_cls;
    (void)bool_cls;
}

void build_class_table(ASTNode *program) {
    ASTList *entry;

    if (program == NULL || program->kind != AST_PROGRAM) {
        semant_error(program, "root of semantic analysis is not AST_PROGRAM");
        return;
    }

    for (entry = program->list; entry != NULL; entry = entry->next) {
        ASTNode *cls_node = entry->node;
        if (cls_node == NULL || cls_node->kind != AST_CLASS) continue;

        if (cls_node->text == NULL) {
            semant_error(cls_node, "class without a name");
            continue;
        }
        if (is_self_type_name(cls_node->text)) {
            semant_error(cls_node, "class name may not be SELF_TYPE");
            continue;
        }
        if (find_class(cls_node->text) != NULL) {
            semant_error(cls_node, "class '%s' is defined more than once", cls_node->text);
            continue;
        }
        new_class(cls_node->text,
                  cls_node->text2 != NULL ? cls_node->text2 : OBJECT_TYPE,
                  cls_node,
                  0);
    }
}

static int validate_declared_type(ASTNode *node,
                                  const char *type_name,
                                  int allow_self_type,
                                  const char *what) {
    if (type_name == NULL) {
        semant_error(node, "%s is missing a type", what);
        return 0;
    }
    if (is_self_type_name(type_name) && !allow_self_type) {
        semant_error(node, "%s may not have type SELF_TYPE", what);
        return 0;
    }
    if (!is_valid_type(type_name)) {
        semant_error(node, "%s uses undefined type '%s'", what, type_name);
        return 0;
    }
    return 1;
}

static void check_inheritance_dfs(ClassInfo *cls) {
    ClassInfo *parent;

    if (cls == NULL) return;
    if (cls->inheritance_state == 2) return;
    if (cls->inheritance_state == 1) {
        semant_error(cls->node, "inheritance cycle involving class '%s'", cls->name);
        return;
    }

    cls->inheritance_state = 1;

    if (cls->parent_name != NULL) {
        if (is_self_type_name(cls->parent_name)) {
            semant_error(cls->node, "class '%s' may not inherit from SELF_TYPE", cls->name);
        } else if (is_basic_type_name(cls->parent_name)) {
            semant_error(cls->node, "class '%s' may not inherit from '%s'", cls->name, cls->parent_name);
        } else {
            parent = find_class(cls->parent_name);
            if (parent == NULL) {
                semant_error(cls->node, "class '%s' inherits from undefined class '%s'",
                             cls->name, cls->parent_name);
            } else {
                check_inheritance_dfs(parent);
            }
        }
    }

    cls->inheritance_state = 2;
}

void check_inheritance(void) {
    ClassInfo *cls;
    for (cls = g_classes; cls != NULL; cls = cls->next) {
        /* Built-ins are already well-formed. */
        if (cls->builtin) {
            cls->inheritance_state = 2;
            continue;
        }
        check_inheritance_dfs(cls);
    }
}

/* ------------------------------------------------------------------------- */
/* Feature collection                                                         */
/* ------------------------------------------------------------------------- */

static int formal_name_exists(FormalInfo *list, const char *name) {
    FormalInfo *f;
    for (f = list; f != NULL; f = f->next) {
        if (streq(f->name, name)) return 1;
    }
    return 0;
}

void collect_features(void) {
    ClassInfo *cls;

    for (cls = g_classes; cls != NULL; cls = cls->next) {
        ASTList *entry;
        if (cls->builtin || cls->node == NULL) continue;

        for (entry = cls->node->list; entry != NULL; entry = entry->next) {
            ASTNode *feature = entry->node;
            if (feature == NULL) continue;

            if (feature->kind == AST_ATTRIBUTE) {
                const char *attr_name = feature->text;
                const char *attr_type = feature->text2;
                if (attr_name == NULL) {
                    semant_error(feature, "attribute without a name in class '%s'", cls->name);
                    continue;
                }
                if (streq(attr_name, "self")) {
                    semant_error(feature, "attribute in class '%s' may not be named self", cls->name);
                    continue;
                }
                if (find_attr_in_class_only(cls, attr_name) != NULL) {
                    semant_error(feature, "attribute '%s' is multiply defined in class '%s'",
                                 attr_name, cls->name);
                    continue;
                }
                (void)validate_declared_type(feature, attr_type, 1, "attribute");
                class_add_attr(cls, new_attr(attr_name,
                                             is_valid_type(attr_type) ? attr_type : OBJECT_TYPE,
                                             feature,
                                             0));
            } else if (feature->kind == AST_METHOD) {
                const char *method_name = feature->text;
                const char *return_type = feature->text2;
                FormalInfo *formals = NULL;
                int formal_count = 0;
                ASTList *formal_entry;

                if (method_name == NULL) {
                    semant_error(feature, "method without a name in class '%s'", cls->name);
                    continue;
                }
                if (find_method_in_class_only(cls, method_name) != NULL) {
                    semant_error(feature, "method '%s' is multiply defined in class '%s'",
                                 method_name, cls->name);
                    continue;
                }
                (void)validate_declared_type(feature, return_type, 1, "method return type");

                for (formal_entry = feature->list; formal_entry != NULL; formal_entry = formal_entry->next) {
                    ASTNode *formal_node = formal_entry->node;
                    const char *formal_name;
                    const char *formal_type;
                    if (formal_node == NULL || formal_node->kind != AST_FORMAL) continue;

                    formal_name = formal_node->text;
                    formal_type = formal_node->text2;
                    if (formal_name == NULL) {
                        semant_error(formal_node, "formal parameter without a name in method '%s'",
                                     method_name);
                        continue;
                    }
                    if (streq(formal_name, "self")) {
                        semant_error(formal_node,
                                     "formal parameter of method '%s' may not be named self",
                                     method_name);
                        continue;
                    }
                    if (formal_name_exists(formals, formal_name)) {
                        semant_error(formal_node,
                                     "formal parameter '%s' is multiply defined in method '%s'",
                                     formal_name, method_name);
                        continue;
                    }
                    (void)validate_declared_type(formal_node, formal_type, 0, "formal parameter");
                    formals = formal_append(
                        formals,
                        new_formal(formal_name,
                                   (formal_type != NULL && is_valid_type(formal_type) &&
                                    !is_self_type_name(formal_type))
                                       ? formal_type
                                       : OBJECT_TYPE));
                    ++formal_count;
                }

                class_add_method(cls,
                                 new_method(method_name,
                                            (return_type != NULL && is_valid_type(return_type))
                                                ? return_type
                                                : OBJECT_TYPE,
                                            formals,
                                            formal_count,
                                            feature,
                                            0));
            }
        }
    }
}

static int same_signature(const MethodInfo *a, const MethodInfo *b) {
    const FormalInfo *fa;
    const FormalInfo *fb;

    if (a == NULL || b == NULL) return 0;
    if (!streq(a->return_type, b->return_type)) return 0;
    if (a->formal_count != b->formal_count) return 0;

    fa = a->formals;
    fb = b->formals;
    while (fa != NULL && fb != NULL) {
        if (!streq(fa->type, fb->type)) return 0;
        fa = fa->next;
        fb = fb->next;
    }
    return fa == NULL && fb == NULL;
}

static void build_method_attr_tables_for_class(ClassInfo *cls) {
    AttrInfo *attr;
    MethodInfo *method;

    if (cls == NULL) return;
    if (cls->feature_state == 2) return;
    if (cls->feature_state == 1) return;

    cls->feature_state = 1;
    if (cls->parent_name != NULL) {
        build_method_attr_tables_for_class(find_class(cls->parent_name));
    }

    for (attr = cls->attrs; attr != NULL; attr = attr->next) {
        AttrInfo *inherited = find_attr_in_ancestors(cls, attr->name);
        if (inherited != NULL) {
            semant_error(attr->node,
                         "attribute '%s' in class '%s' redefines inherited attribute",
                         attr->name, cls->name);
        }
    }

    for (method = cls->methods; method != NULL; method = method->next) {
        MethodInfo *inherited = find_method_in_ancestors(cls, method->name);
        if (inherited != NULL && !same_signature(method, inherited)) {
            semant_error(method->node,
                         "method '%s' in class '%s' does not match inherited signature exactly",
                         method->name, cls->name);
        }
    }

    cls->feature_state = 2;
}

void build_method_attr_tables(void) {
    ClassInfo *cls;
    for (cls = g_classes; cls != NULL; cls = cls->next) {
        build_method_attr_tables_for_class(cls);
    }
}

/* ------------------------------------------------------------------------- */
/* Scope stack                                                                */
/* ------------------------------------------------------------------------- */

void scope_enter(void) {
    Scope *scope = (Scope *)xcalloc(1U, sizeof(*scope));
    scope->next = g_scope_stack;
    g_scope_stack = scope;
}

void scope_exit(void) {
    Scope *scope;
    VarBinding *binding;
    VarBinding *next;

    if (g_scope_stack == NULL) return;
    scope = g_scope_stack;
    g_scope_stack = scope->next;

    binding = scope->bindings;
    while (binding != NULL) {
        next = binding->next;
        free(binding->name);
        free(binding->type);
        free(binding);
        binding = next;
    }
    free(scope);
}

void scope_add(const char *name, const char *type) {
    VarBinding *binding;
    if (g_scope_stack == NULL) scope_enter();

    binding = (VarBinding *)xcalloc(1U, sizeof(*binding));
    binding->name = xstrdup(name);
    binding->type = xstrdup(type);
    binding->next = g_scope_stack->bindings;
    g_scope_stack->bindings = binding;
}

const char *scope_lookup(const char *name) {
    Scope *scope;
    for (scope = g_scope_stack; scope != NULL; scope = scope->next) {
        VarBinding *binding;
        for (binding = scope->bindings; binding != NULL; binding = binding->next) {
            if (streq(binding->name, name)) return binding->type;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Type operations                                                            */
/* ------------------------------------------------------------------------- */

static const char *resolve_self_type(const char *type_name, const char *current_class) {
    if (is_self_type_name(type_name)) return current_class;
    return type_name;
}

int conforms(const char *child, const char *parent, const char *current_class) {
    const char *resolved_child;
    const char *resolved_parent;
    ClassInfo *cursor;

    if (child == NULL || parent == NULL) return 0;
    if (streq(child, parent)) return 1;

    if (is_self_type_name(parent)) {
        /* Only SELF_TYPE_C <= SELF_TYPE_C is allowed; exact equality handled above. */
        return 0;
    }

    resolved_child = resolve_self_type(child, current_class);
    resolved_parent = resolve_self_type(parent, current_class);
    if (resolved_child == NULL || resolved_parent == NULL) return 0;
    if (streq(resolved_child, resolved_parent)) return 1;

    cursor = find_class(resolved_child);
    while (cursor != NULL && cursor->parent_name != NULL) {
        if (streq(cursor->parent_name, resolved_parent)) return 1;
        cursor = find_class(cursor->parent_name);
    }
    return 0;
}

static int depth_to_root(const char *type_name, const char *current_class) {
    int depth = 0;
    ClassInfo *cursor = find_class(resolve_self_type(type_name, current_class));
    while (cursor != NULL) {
        ++depth;
        cursor = cursor->parent_name != NULL ? find_class(cursor->parent_name) : NULL;
    }
    return depth;
}

const char *join_types(const char *a, const char *b, const char *current_class) {
    const char *ra;
    const char *rb;
    ClassInfo *ca;
    ClassInfo *cb;
    int da;
    int db;

    if (a == NULL || b == NULL) return OBJECT_TYPE;
    if (streq(a, b)) return a;
    if (is_self_type_name(a) && is_self_type_name(b)) return SELF_TYPE_NAME;

    ra = resolve_self_type(a, current_class);
    rb = resolve_self_type(b, current_class);
    if (ra == NULL || rb == NULL) return OBJECT_TYPE;
    if (streq(ra, rb)) return ra;

    ca = find_class(ra);
    cb = find_class(rb);
    if (ca == NULL || cb == NULL) return OBJECT_TYPE;

    da = depth_to_root(ra, current_class);
    db = depth_to_root(rb, current_class);

    while (da > db && ca != NULL) {
        ca = ca->parent_name != NULL ? find_class(ca->parent_name) : NULL;
        --da;
    }
    while (db > da && cb != NULL) {
        cb = cb->parent_name != NULL ? find_class(cb->parent_name) : NULL;
        --db;
    }
    while (ca != NULL && cb != NULL && !streq(ca->name, cb->name)) {
        ca = ca->parent_name != NULL ? find_class(ca->parent_name) : NULL;
        cb = cb->parent_name != NULL ? find_class(cb->parent_name) : NULL;
    }
    return ca != NULL ? ca->name : OBJECT_TYPE;
}

/* ------------------------------------------------------------------------- */
/* Scope population helpers                                                   */
/* ------------------------------------------------------------------------- */

static void add_attributes_to_scope_recursive(ClassInfo *cls) {
    AttrInfo *attr;
    if (cls == NULL) return;
    if (cls->parent_name != NULL) {
        add_attributes_to_scope_recursive(find_class(cls->parent_name));
    }
    for (attr = cls->attrs; attr != NULL; attr = attr->next) {
        scope_add(attr->name, attr->type);
    }
}

static int stringset_contains(StringSet *set, const char *value) {
    while (set != NULL) {
        if (streq(set->value, value)) return 1;
        set = set->next;
    }
    return 0;
}

static StringSet *stringset_add(StringSet *set, const char *value) {
    StringSet *node = (StringSet *)xcalloc(1U, sizeof(*node));
    node->value = xstrdup(value);
    node->next = set;
    return node;
}

static void stringset_free(StringSet *set) {
    StringSet *next;
    while (set != NULL) {
        next = set->next;
        free(set->value);
        free(set);
        set = next;
    }
}

/* ------------------------------------------------------------------------- */
/* Expression checking                                                        */
/* ------------------------------------------------------------------------- */

static const char *checked_declared_type(ASTNode *node,
                                         const char *type_name,
                                         int allow_self_type,
                                         const char *what) {
    if (validate_declared_type(node, type_name, allow_self_type, what)) return type_name;
    return OBJECT_TYPE;
}

static void require_type(ASTNode *where,
                         const char *got,
                         const char *expected,
                         const char *context,
                         const char *current_class) {
    if (!conforms(got, expected, current_class)) {
        semant_error(where, "%s has type '%s' but expected '%s'",
                     context,
                     got != NULL ? got : "<unknown>",
                     expected != NULL ? expected : "<unknown>");
    }
}

const char *check_expr(ASTNode *node, const char *current_class) {
    ClassInfo *current_cls;

    if (node == NULL) return OBJECT_TYPE;
    current_cls = find_class(current_class);
    (void)current_cls;

    switch (node->kind) {
        case AST_INTEGER:
            set_inferred_type(node, INT_TYPE);
            return node->inferred_type;

        case AST_STRING:
            set_inferred_type(node, STRING_TYPE);
            return node->inferred_type;

        case AST_BOOLEAN:
            set_inferred_type(node, BOOL_TYPE);
            return node->inferred_type;

        case AST_IDENTIFIER: {
            const char *type = scope_lookup(node->text);
            if (type == NULL) {
                semant_error(node, "use of undefined identifier '%s'",
                             node->text != NULL ? node->text : "<unnamed>");
                type = OBJECT_TYPE;
            }
            
            /*
                if (type == NULL) {
                free(node->text);
                node->text = xstrdup("0");

                free(node->text2);
                node->text2 = NULL;

                node->kind = AST_INTEGER;

                set_inferred_type(node, INT_TYPE);
                return node->inferred_type;
            }
            
            */
            set_inferred_type(node, type);
            return node->inferred_type;
        }

        case AST_NEW: {
            const char *type_name = checked_declared_type(node, node->text, 1, "new expression");
            set_inferred_type(node, type_name);
            return node->inferred_type;
        }

        case AST_BLOCK: {
            ASTList *entry;
            const char *last_type = OBJECT_TYPE;
            for (entry = node->list; entry != NULL; entry = entry->next) {
                last_type = check_expr(entry->node, current_class);
            }
            set_inferred_type(node, last_type);
            return node->inferred_type;
        }

        case AST_PLUS:
        case AST_MINUS:
        case AST_MULTIPLY:
        case AST_DIVIDE:
        case AST_POWER: {
            const char *left = check_expr(node->child1, current_class);
            const char *right = check_expr(node->child2, current_class);
            require_type(node->child1, left, INT_TYPE, "left operand", current_class);
            require_type(node->child2, right, INT_TYPE, "right operand", current_class);
            set_inferred_type(node, INT_TYPE);
            return node->inferred_type;
        }

        case AST_NEGATE: {
            const char *t = check_expr(node->child1, current_class);
            require_type(node->child1, t, INT_TYPE, "operand of ~", current_class);
            set_inferred_type(node, INT_TYPE);
            return node->inferred_type;
        }

        case AST_LESS_THAN:
        case AST_LESS_EQUAL: {
            const char *left = check_expr(node->child1, current_class);
            const char *right = check_expr(node->child2, current_class);
            require_type(node->child1, left, INT_TYPE, "left comparison operand", current_class);
            require_type(node->child2, right, INT_TYPE, "right comparison operand", current_class);
            set_inferred_type(node, BOOL_TYPE);
            return node->inferred_type;
        }

        case AST_EQUAL: {
            const char *left = check_expr(node->child1, current_class);
            const char *right = check_expr(node->child2, current_class);
            const char *rleft = resolve_self_type(left, current_class);
            const char *rright = resolve_self_type(right, current_class);

            if ((is_basic_type_name(rleft) || is_basic_type_name(rright)) && !streq(rleft, rright)) {
                semant_error(node,
                             "illegal equality test between '%s' and '%s'",
                             left != NULL ? left : "<unknown>",
                             right != NULL ? right : "<unknown>");
            }
            set_inferred_type(node, BOOL_TYPE);
            return node->inferred_type;
        }

        case AST_NOT: {
            const char *t = check_expr(node->child1, current_class);
            require_type(node->child1, t, BOOL_TYPE, "operand of not", current_class);
            set_inferred_type(node, BOOL_TYPE);
            return node->inferred_type;
        }

        case AST_ISVOID:
            (void)check_expr(node->child1, current_class);
            set_inferred_type(node, BOOL_TYPE);
            return node->inferred_type;

        case AST_IF: {
            const char *pred = check_expr(node->child1, current_class);
            const char *then_t = check_expr(node->child2, current_class);
            const char *else_t = check_expr(node->child3, current_class);
            require_type(node->child1, pred, BOOL_TYPE, "if predicate", current_class);
            set_inferred_type(node, join_types(then_t, else_t, current_class));
            return node->inferred_type;
        }

        case AST_WHILE: {
            const char *pred = check_expr(node->child1, current_class);
            (void)check_expr(node->child2, current_class);
            require_type(node->child1, pred, BOOL_TYPE, "while predicate", current_class);
            set_inferred_type(node, OBJECT_TYPE);
            return node->inferred_type;
        }

        case AST_ASSIGN: {
            const char *declared_type;
            const char *expr_type;

            if (streq(node->text, "self")) {
                semant_error(node, "cannot assign to self");
            }
            declared_type = scope_lookup(node->text);
            expr_type = check_expr(node->child1, current_class);
            if (declared_type == NULL) {
                semant_error(node, "assignment to undefined identifier '%s'",
                             node->text != NULL ? node->text : "<unnamed>");
                declared_type = OBJECT_TYPE;
            }
            if (!conforms(expr_type, declared_type, current_class)) {
                semant_error(node,
                             "type '%s' of assigned expression does not conform to declared type '%s' of '%s'",
                             expr_type != NULL ? expr_type : "<unknown>",
                             declared_type,
                             node->text != NULL ? node->text : "<unnamed>");
            }
            set_inferred_type(node, expr_type != NULL ? expr_type : OBJECT_TYPE);
            return node->inferred_type;
        }
        
        
                        
                case AST_REPIT_UNTIL: {
                    const char *condition_type;

                    (void)check_expr(node->child1, current_class);
                    condition_type = check_expr(node->child2, current_class);

                    require_type(node->child2,
                                condition_type,
                                BOOL_TYPE,
                                "repeat-until predicate",
                                current_class);

                    set_inferred_type(node, OBJECT_TYPE);
                    return node->inferred_type;
                }
        
        

        case AST_LET: {
            ASTList *entry;
            const char *body_type;
            scope_enter();
            for (entry = node->list; entry != NULL; entry = entry->next) {
                ASTNode *binding = entry->node;
                const char *declared_type;
                if (binding == NULL || binding->kind != AST_LET_BINDING) continue;

                declared_type = checked_declared_type(binding, binding->text2, 1, "let binding");
                if (streq(binding->text, "self")) {
                    semant_error(binding, "let binding may not use identifier self");
                }
                if (binding->child1 != NULL) {
                    const char *init_type = check_expr(binding->child1, current_class);
                    if (!conforms(init_type, declared_type, current_class)) {
                        semant_error(binding,
                                     "initializer type '%s' does not conform to declared let type '%s'",
                                     init_type != NULL ? init_type : "<unknown>",
                                     declared_type);
                    }
                }
                if (!streq(binding->text, "self")) {
                    scope_add(binding->text != NULL ? binding->text : "<bad-let>", declared_type);
                }
            }
            body_type = check_expr(node->child1, current_class);
            scope_exit();
            set_inferred_type(node, body_type);
            return node->inferred_type;
        }

        case AST_CASE: {
            ASTList *entry;
            StringSet *branch_types = NULL;
            const char *result_type = NULL;
            (void)check_expr(node->child1, current_class);

            for (entry = node->list; entry != NULL; entry = entry->next) {
                ASTNode *branch = entry->node;
                const char *declared_type;
                const char *branch_result;
                if (branch == NULL || branch->kind != AST_CASE_BRANCH) continue;

                declared_type = checked_declared_type(branch, branch->text2, 0, "case branch");
                if (stringset_contains(branch_types, declared_type)) {
                    semant_error(branch,
                                 "duplicate branch type '%s' in case expression",
                                 declared_type);
                } else {
                    branch_types = stringset_add(branch_types, declared_type);
                }
                if (streq(branch->text, "self")) {
                    semant_error(branch, "case branch variable may not be named self");
                }
                scope_enter();
                if (!streq(branch->text, "self")) {
                    scope_add(branch->text != NULL ? branch->text : "<bad-case>", declared_type);
                }
                branch_result = check_expr(branch->child1, current_class);
                scope_exit();

                if (result_type == NULL) {
                    result_type = branch_result;
                } else {
                    result_type = join_types(result_type, branch_result, current_class);
                }
            }

            stringset_free(branch_types);
            set_inferred_type(node, result_type != NULL ? result_type : OBJECT_TYPE);
            return node->inferred_type;
        }

        case AST_DISPATCH:
        case AST_STATIC_DISPATCH:
        case AST_SELF_DISPATCH: {
            const char *receiver_type = NULL;
            const char *lookup_type = NULL;
            const char *result_type = OBJECT_TYPE;
            ClassInfo *lookup_class = NULL;
            MethodInfo *method = NULL;
            ASTList *arg_entry;
            FormalInfo *formal;
            int actual_count = 0;
            int expected_count = 0;

            if (node->kind == AST_SELF_DISPATCH) {
                receiver_type = SELF_TYPE_NAME;
                lookup_type = current_class;
            } else {
                receiver_type = check_expr(node->child1, current_class);
                lookup_type = resolve_self_type(receiver_type, current_class);
            }

            if (node->kind == AST_STATIC_DISPATCH) {
                const char *static_type = checked_declared_type(node, node->text2, 0,
                                                               "static dispatch type");
                if (!conforms(receiver_type, static_type, current_class)) {
                    semant_error(node,
                                 "expression type '%s' does not conform to static dispatch type '%s'",
                                 receiver_type != NULL ? receiver_type : "<unknown>",
                                 static_type);
                }
                lookup_type = static_type;
            }

            lookup_class = find_class(lookup_type);
            if (lookup_class == NULL) {
                semant_error(node,
                             "dispatch uses undefined class '%s'",
                             lookup_type != NULL ? lookup_type : "<unknown>");
                set_inferred_type(node, OBJECT_TYPE);
                return node->inferred_type;
            }

            method = lookup_method(lookup_class, node->text);
            if (method == NULL) {
                semant_error(node,
                             "undefined method '%s' for class '%s'",
                             node->text != NULL ? node->text : "<unnamed>",
                             lookup_class->name);
                for (arg_entry = node->list; arg_entry != NULL; arg_entry = arg_entry->next) {
                    (void)check_expr(arg_entry->node, current_class);
                }
                set_inferred_type(node, OBJECT_TYPE);
                return node->inferred_type;
            }

            formal = method->formals;
            expected_count = method->formal_count;
            for (arg_entry = node->list; arg_entry != NULL; arg_entry = arg_entry->next) {
                const char *arg_type = check_expr(arg_entry->node, current_class);
                ++actual_count;
                if (formal != NULL) {
                    if (!conforms(arg_type, formal->type, current_class)) {
                        semant_error(arg_entry->node,
                                     "argument %d of method '%s' has type '%s' but expected '%s'",
                                     actual_count,
                                     method->name,
                                     arg_type != NULL ? arg_type : "<unknown>",
                                     formal->type);
                    }
                    formal = formal->next;
                }
            }
            if (actual_count != expected_count) {
                semant_error(node,
                             "method '%s' called with %d argument(s), but %d expected",
                             method->name, actual_count, expected_count);
            }

            if (streq(method->return_type, SELF_TYPE_NAME)) {
                result_type = receiver_type != NULL ? receiver_type : OBJECT_TYPE;
            } else {
                result_type = method->return_type;
            }
            set_inferred_type(node, result_type);
            return node->inferred_type;
        }

        case AST_ERROR:
            set_inferred_type(node, OBJECT_TYPE);
            return node->inferred_type;

        default:
            semant_error(node, "unhandled AST node kind %d in semantic analysis", (int)node->kind);
            set_inferred_type(node, OBJECT_TYPE);
            return node->inferred_type;
    }
}

/* ------------------------------------------------------------------------- */
/* Class/body checking                                                        */
/* ------------------------------------------------------------------------- */

static void check_attribute_initializers(ClassInfo *cls) {
    AttrInfo *attr;

    scope_enter();
    scope_add("self", SELF_TYPE_NAME);
    add_attributes_to_scope_recursive(cls);

    for (attr = cls->attrs; attr != NULL; attr = attr->next) {
        if (attr->node != NULL && attr->node->child1 != NULL) {
            const char *init_type = check_expr(attr->node->child1, cls->name);
            if (!conforms(init_type, attr->type, cls->name)) {
                semant_error(attr->node,
                             "initializer type '%s' of attribute '%s' does not conform to declared type '%s'",
                             init_type != NULL ? init_type : "<unknown>",
                             attr->name,
                             attr->type);
            }
        }
    }

    scope_exit();
}

static void check_method_bodies(ClassInfo *cls) {
    MethodInfo *method;

    scope_enter();
    scope_add("self", SELF_TYPE_NAME);
    add_attributes_to_scope_recursive(cls);

    for (method = cls->methods; method != NULL; method = method->next) {
        FormalInfo *formal;
        const char *body_type;

        if (method->node == NULL) continue; /* built-in */

        scope_enter();
        for (formal = method->formals; formal != NULL; formal = formal->next) {
            scope_add(formal->name, formal->type);
        }

        body_type = check_expr(method->node->child1, cls->name);
        if (!conforms(body_type, method->return_type, cls->name)) {
            semant_error(method->node,
                         "body type '%s' of method '%s' does not conform to declared return type '%s'",
                         body_type != NULL ? body_type : "<unknown>",
                         method->name,
                         method->return_type);
        }

        scope_exit();
    }

    scope_exit();
}

static void check_main_class_and_method(void) {
    ClassInfo *main_cls = find_class(MAIN_CLASS);
    MethodInfo *main_method;

    if (main_cls == NULL || main_cls->builtin) {
        semant_error(NULL, "program must define class Main");
        return;
    }

    main_method = find_method_in_class_only(main_cls, MAIN_METHOD);
    if (main_method == NULL) {
        semant_error(main_cls->node,
                     "class Main must define method main with no formal parameters");
        return;
    }
    if (main_method->formal_count != 0) {
        semant_error(main_method->node,
                     "method Main.main must take no formal parameters");
    }
}

static void check_program_bodies(void) {
    ClassInfo *cls;
    for (cls = g_classes; cls != NULL; cls = cls->next) {
        if (cls->builtin) continue;
        check_attribute_initializers(cls);
        check_method_bodies(cls);
    }
}

/* ------------------------------------------------------------------------- */
/* Public entry point                                                         */
/* ------------------------------------------------------------------------- */

static void print_formals(const FormalInfo *formals, FILE *out) {
    const FormalInfo *formal;

    for (formal = formals; formal != NULL; formal = formal->next) {
        fprintf(out, "%s : %s", formal->name, formal->type);
    }
}

void semant_print_symbol_table(FILE *out) {
    ClassInfo *cls;

    fputs("\nSymbolTable\n", out);
    for (cls = g_classes; cls != NULL; cls = cls->next) {
        AttrInfo *attr;
        MethodInfo *method;

        if (cls->builtin) continue;

        fprintf(out, "  Class %s", cls->name);
        if (cls->parent_name != NULL) {
            fprintf(out, " inherits %s", cls->parent_name);
        }
        fputc('\n', out);

        for (attr = cls->attrs; attr != NULL; attr = attr->next) {
            fprintf(out, "    attr %s : %s\n", attr->name, attr->type);
        }

        for (method = cls->methods; method != NULL; method = method->next) {
            fprintf(out, "    method %s(", method->name);
            print_formals(method->formals, out);
            fprintf(out, ") : %s\n", method->return_type);
        }
    }
}

int semant_check(ASTNode *program) {
    int hierarchy_errors_before;

    g_classes = NULL;
    g_scope_stack = NULL;
    g_semant_errors = 0;

    add_builtin_classes();
    build_class_table(program);

    hierarchy_errors_before = g_semant_errors;
    check_inheritance();
    if (g_semant_errors != hierarchy_errors_before) {
        /* Per PA3 guidance, a malformed inheritance graph is a hard stop. */
        return g_semant_errors;
    }

    collect_features();
    build_method_attr_tables();
    check_main_class_and_method();
    check_program_bodies();

    return g_semant_errors;
}
