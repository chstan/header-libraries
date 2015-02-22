#ifndef CSCHEME_H
#define CSCHEME_H

#define SCHEME_GC_DEBUG

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cparse.h"
#include "cbignum.h"
#include "clex.h"

typedef enum {
    SCHEME_EMPTY_LIST,          // done
    SCHEME_BOOLEAN,             // done
    SCHEME_NUMBER,              // done
    SCHEME_SYMBOL,              // done
    SCHEME_CHARACTER,           // done
    SCHEME_STRING,              // done
    SCHEME_VECTOR,              // done
    SCHEME_PAIR,                // not done
    SCHEME_PRIMITIVE_PROCEDURE, // done
    SCHEME_COMPOUND_PROCEDURE   // done
} SchemeObjectType;

struct SchemeObject;
struct SchemeEnv;

typedef struct SchemeObject *(*SchemeProcedure)(struct SchemeObject *);
typedef struct SchemeObject *(*SchemeSpecialForm)(struct SchemeEnv *,
                                                  struct SchemeObject *);

typedef struct SchemeObject {
    SchemeObjectType _type;
    union {
        struct {
            bool _value;
        } _boolean;
        struct {
            Bignum *_value;
        } _number;
        struct {
            char *_value;
        } _symbol;
        struct {
            char _value;
        } _character;
        struct {
            char *_value;
        } _string;
        struct {
            Vector *_value;
        } _vector;
        struct {
            struct SchemeObject *_car;
            struct SchemeObject *_cdr;
        } _pair;
        struct {
            SchemeProcedure _fn;
            char *_proc_name;
            size_t _n_args_expected;
        } _primitive_procedure;
        struct {
            struct SchemeObject *_body;
            struct SchemeObject *_env;
            Vector *_req_args;
            Vector *_opt_args;
            struct SchemeObject *_rest;
            char *_proc_name;
            size_t _n_args_expected;
        } _compound_procedure;
    } _data;
} SchemeObject;

void arity_check(SchemeObject *f, SchemeObject *args);


void set_car(SchemeObject *l, SchemeObject *r) {
    l->_data._pair._car = r;
}

void set_cdr(SchemeObject *l, SchemeObject *r) {
    l->_data._pair._cdr = r;
}

SchemeObject *car(SchemeObject *o) {
    return o->_data._pair._car;
}

SchemeObject *cdr(SchemeObject *o) {
    return o->_data._pair._cdr;
}

void scheme_object_fprint(FILE *f, SchemeObject *o);

void scheme_vector_fprint(FILE *f, SchemeObject *o) {
    assert(o->_type == SCHEME_VECTOR);
    fprintf(f, "#(");
    Vector *v = o->_data._vector._value;
    for (size_t idx = 0; idx < v_size(v); idx++) {
        scheme_object_fprint(f, *((SchemeObject **) v_at(v, idx)));
        if (idx != v_size(v) - 1) {
            fprintf(f, " ");
        }
    }
    fprintf(f, ")");
}

void scheme_character_fprint(FILE *f, SchemeObject *o) {
    assert(o->_type == SCHEME_CHARACTER);
    fprintf(f, "#\\");
    switch(o->_data._character._value) {
    case '\n':
        fprintf(f, "newline");
        break;
    case ' ':
        fprintf(f, "space");
        break;
    default:
        fprintf(f, "%c", o->_data._character._value);
        break;
    }
}

bool scheme_is_list(SchemeObject *o) {
    if (o->_type == SCHEME_EMPTY_LIST) return true;
    if (o->_type != SCHEME_PAIR) return false;
    if (cdr(o)->_type == SCHEME_EMPTY_LIST) return true;
    if (cdr(o)->_type == SCHEME_PAIR) return true;
    return false;
}

bool scheme_truthy(SchemeObject *obj) {
    if (obj->_type == SCHEME_BOOLEAN && obj->_data._boolean._value == false) {
        return false;
    } else {
        return true;
    }
}

SchemeObject *scheme_pun_object_proc(SchemeObject *obj) {
    SchemeObject *out = (SchemeObject *) malloc(sizeof(SchemeObject));
    out->_type = SCHEME_BOOLEAN;
    out->_data._boolean._value = scheme_truthy(obj);
    return out;
}

bool scheme_quoted_variety(SchemeObject *o) {
    if (o->_type == SCHEME_SYMBOL) {
        char *s = o->_data._symbol._value;
        if(strcmp(s, "quote") == 0) return true;
        if(strcmp(s, "quasiquote") == 0) return true;
        if(strcmp(s, "unquote") == 0) return true;
        if(strcmp(s, "unquote-splicing") == 0) return true;
    }
    return false;
}

bool scheme_is_quote_variety_list(SchemeObject *o) {
    if (!scheme_is_list(o)) return false;
    return scheme_quoted_variety(car(o));
}

size_t scheme_length(SchemeObject *obj) {
    assert(scheme_is_list(obj));
    if (obj->_type == SCHEME_EMPTY_LIST) return 0;
    return scheme_length(cdr(obj)) + 1;
}

void scheme_list_fprint(FILE *f, SchemeObject *o, bool parens);

void scheme_quoted_variety_fprint(FILE *f, SchemeObject *o) {
    char *s = o->_data._symbol._value;
    if(strcmp(s, "quote") == 0) fprintf(f, "'");
    if(strcmp(s, "quasiquote") == 0) fprintf(f, "`");
    if(strcmp(s, "unquote") == 0) fprintf(f, ",");
    if(strcmp(s, "unquote-splicing") == 0) fprintf(f, ",@");
}

void scheme_quoted_list_fprint(FILE *f, SchemeObject *o) {
    scheme_quoted_variety_fprint(f, car(o));
    scheme_list_fprint(f, cdr(o), true);
}

void scheme_list_fprint(FILE *f, SchemeObject *o, bool parens) {
    if (o->_type == SCHEME_EMPTY_LIST) {
        if (parens) fprintf(f, "()");
        return;
    }
    assert(o->_type == SCHEME_PAIR);
    if (scheme_is_quote_variety_list(o)) {
        scheme_quoted_list_fprint(f, o);
        return;
    }
    if (parens) fprintf(f, "(");
    scheme_object_fprint(f, car(o));
    if (cdr(o)->_type != SCHEME_EMPTY_LIST)
        fprintf(f, " ");
    scheme_list_fprint(f, cdr(o), false);
    if (parens) fprintf(f, ")");
}

void scheme_pair_fprint(FILE *f, SchemeObject *o) {
    assert(o->_type == SCHEME_PAIR);
    if (scheme_quoted_variety(car(o))) {
        scheme_quoted_variety_fprint(f, car(o));
        scheme_object_fprint(f, cdr(o));
        return;
    }
    fprintf(f, "(");
    scheme_object_fprint(f, o->_data._pair._car);
    if (cdr(o)->_type != SCHEME_EMPTY_LIST) {
        if (scheme_is_list(cdr(o))) {

        } else {
            fprintf(f, " . ");
            scheme_object_fprint(f, o->_data._pair._cdr);
        }
    }
    fprintf(f, ")");
}

void scheme_object_fprint(FILE *f, SchemeObject *o) {
    switch(o->_type) {
    case SCHEME_EMPTY_LIST:
        fprintf(f, "()");
        break;
    case SCHEME_BOOLEAN:
        fprintf(f, "#%c", o->_data._character._value == false ? 'f' : 't');
        break;
    case SCHEME_NUMBER:
        bn_fprint(f, o->_data._number._value);
        break;
    case SCHEME_STRING:
        fprintf(f, "\"%s\"", o->_data._string._value);
        break;
    case SCHEME_SYMBOL:
        fprintf(f, "%s", o->_data._symbol._value);
        break;
    case SCHEME_CHARACTER:
        scheme_character_fprint(f, o);
        break;
    case SCHEME_VECTOR:
        scheme_vector_fprint(f, o);
        break;
    case SCHEME_PAIR: {
        if (scheme_is_list(o))
            scheme_list_fprint(f, o, true);
        else
            scheme_pair_fprint(f, o);
        break;
    }
    case SCHEME_PRIMITIVE_PROCEDURE:
        fprintf(f, "#[primitive-procedure %s]\n",
                o->_data._primitive_procedure._proc_name);
        break;
    case SCHEME_COMPOUND_PROCEDURE:
        fprintf(f, "#[compound-procedure %s]\n",
                o->_data._compound_procedure._proc_name);
        break;
    }
}

void scheme_object_free(SchemeObject *o) {
    switch(o->_type) {
    case SCHEME_EMPTY_LIST:
    case SCHEME_BOOLEAN:
    case SCHEME_CHARACTER:
        free(o);
        break;
    case SCHEME_NUMBER: {
        bn_free(o->_data._number._value);
        free(o);
        break;
    }
    case SCHEME_STRING: {
        free(o->_data._string._value);
        free(o);
        break;
    }
    case SCHEME_SYMBOL: {
        free(o->_data._symbol._value);
        free(o);
        break;
    }
    case SCHEME_VECTOR: {
        v_free(o->_data._vector._value);
        free(o);
        break;
    }
    case SCHEME_PAIR: {
        scheme_object_free(o->_data._pair._car);
        scheme_object_free(o->_data._pair._cdr);
        free(o);
        break;
    }
    case SCHEME_PRIMITIVE_PROCEDURE:
        assert(0);
        break;
    case SCHEME_COMPOUND_PROCEDURE:
        assert(0);
        break;
    }
}

void wrapped_scheme_object_free(void *o, __attribute__((unused)) void *aux) {
    scheme_object_free(*((SchemeObject **) o));
}

bool scheme_significant_token(const void *p, __attribute__((unused)) const void *aux) {
    Token *t = (Token *) p;
    if (strcmp(t->_category->_category_label, "WHITESPACE") == 0) return false;
    if (strcmp(t->_category->_category_label, "COMMENT") == 0) return false;
    return true;
}

typedef struct SchemeEnv {
    LexerEnv *_lexer;
    ParserEnv *_parser;
    Map *_symbol_table;
    Map *_special_form_table;
    Vector *_lexical_environment_stack;
    size_t _n_lambdas;
} SchemeEnv;

SchemeObject *scheme_simple_list_combines_internal(void **data, size_t n) {
    if (n == 0) {
        SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        obj->_type = SCHEME_EMPTY_LIST;
        return obj;
    }
    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_PAIR;
    obj->_data._pair._car = (SchemeObject *) *data;
    obj->_data._pair._cdr = scheme_simple_list_combines_internal(data + 1, n - 1);
    return obj;
}

SchemeObject *scheme_generic_combines_internal(void **data, size_t non_null_elems) {
    if (non_null_elems == 0) {
        SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        obj->_type = SCHEME_EMPTY_LIST;
        return obj;
    }
    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_PAIR;
    while (*data == NULL) data++;
    set_car(obj, (SchemeObject *) *data);
    set_cdr(obj, scheme_simple_list_combines_internal(data + 1, non_null_elems - 1));
    return obj;
}

void *scheme_generic_combines(Vector *v) {
    size_t n_elems = v_size(v);
    if (n_elems == 0) return NULL;
    size_t non_null_elems = 0;
    for (size_t idx = 0; idx < n_elems; idx++)
        if (*(void **) v_at(v, idx) != NULL) non_null_elems++;
    void *o = scheme_generic_combines_internal((void **)v->_data, non_null_elems);
    v_free(v);
    return o;
}

SchemeObject *scheme_last_vec_combines_internal(void **data, size_t non_null_elems) {
    if (non_null_elems == 0) {
        SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        obj->_type = SCHEME_EMPTY_LIST;
        return obj;
    }
    while (*data == NULL) data++;

    if (non_null_elems == 1) {
        Vector *v = (Vector *) *data;
        return (SchemeObject *) scheme_generic_combines(v);
    }

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_PAIR;
    set_car(obj, (SchemeObject *) *data);
    set_cdr(obj, scheme_last_vec_combines_internal(data + 1, non_null_elems - 1));
    return obj;
}

SchemeObject *scheme_generic_combines_with_tail_internal(void **data,
                                                         size_t non_null_elems,
                                                         SchemeObject *tail) {
    if (non_null_elems == 0) {
        SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        SchemeObject *empty = (SchemeObject *) malloc(sizeof(SchemeObject));
        obj->_type = SCHEME_PAIR;
        empty->_type = SCHEME_EMPTY_LIST;
        set_car(obj, tail);
        set_cdr(obj, empty);
        return obj;
    }
    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_PAIR;
    while (*data == NULL) data++;
    set_car(obj, (SchemeObject *) *data);
    set_cdr(obj, scheme_simple_list_combines_internal(data + 1, non_null_elems - 1));
    return obj;
}

SchemeObject *scheme_generic_combines_with_tail(Vector *v, SchemeObject *tail) {
    size_t n_elems = v_size(v);
    if (n_elems == 0) return NULL;
    size_t non_null_elems = 0;
    for (size_t idx = 0; idx < n_elems; idx++)
        if (*(void **) v_at(v, idx) != NULL) non_null_elems++;
    void *o = scheme_generic_combines_with_tail_internal((void **)v->_data, non_null_elems, tail);
    v_free(v);
    return o;
}

void *scheme_case_else_combines(Vector *v) {
    assert(v_size(v) == 6);
    SchemeObject *case_obj = *(SchemeObject **) v_at(v, 1);
    SchemeObject *expr_obj = *(SchemeObject **) v_at(v, 2);
    Vector *clauses = *(Vector **) v_at(v, 3);
    SchemeObject *else_obj = *(SchemeObject **) v_at(v, 4);

    SchemeObject *clauses_else_obj =
        scheme_generic_combines_with_tail(clauses, else_obj);

    Vector *t = v_make(sizeof(void *));
    v_push_back(t, case_obj);
    v_push_back(t, expr_obj);
    v_push_back(t, clauses_else_obj);

    SchemeObject *obj = (SchemeObject *) scheme_generic_combines(t);
    v_free(v);
    return obj;
}

void *scheme_cond_else_combines(Vector *v) {
    assert(v_size(v) == 6);
    SchemeObject *case_obj = *(SchemeObject **) v_at(v, 1);
    Vector *clauses = *(Vector **) v_at(v, 2);
    SchemeObject *else_obj = *(SchemeObject **) v_at(v, 3);

    SchemeObject *clauses_else_obj =
        scheme_generic_combines_with_tail(clauses, else_obj);

    Vector *t = v_make(sizeof(void *));
    v_push_back(t, case_obj);
    v_push_back(t, clauses_else_obj);

    SchemeObject *obj = (SchemeObject *) scheme_generic_combines(t);
    v_free(v);
    return obj;
}

void *scheme_last_vec_combines(Vector *v) {
    size_t n_elems = v_size(v);
    if (n_elems == 0) return NULL;
    size_t non_null_elems = 0;
    for (size_t idx = 0; idx < n_elems; idx++)
        if (*(void **) v_at(v, idx) != NULL) non_null_elems++;
    void *o = scheme_last_vec_combines_internal((void **)v->_data, non_null_elems);
    v_free(v);
    return o;
}

void *scheme_simple_list_combines(Vector *v) {
    assert(v_size(v) == 3);
    Vector *data = *(Vector **) v_at(v, 1);
    SchemeObject *obj =
        scheme_simple_list_combines_internal((void **)data->_data, v_size(data));
    v_free(v);
    v_free(data);
    return obj;
}

SchemeObject *scheme_dotted_list_combines_internal(void **cars, size_t n,
                                                   SchemeObject *cdr) {
    if (n == 1) {
        SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        obj->_type = SCHEME_PAIR;
        set_car(obj, *(SchemeObject **) cars);
        set_cdr(obj, cdr);
        return obj;
    }
    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_PAIR;
    set_car(obj, *(SchemeObject **) cars);
    set_cdr(obj, scheme_dotted_list_combines_internal(cars + 1, n - 1, cdr));
    return obj;
}

void *scheme_dotted_list_combines(Vector *v) {
    assert(v_size(v) == 5);
    Vector *predata = *(Vector **) v_at(v, 1);
    SchemeObject *post_data = *(SchemeObject **) v_at(v, 3);
    assert(v_size(predata) >= 1);
    SchemeObject *obj =
        scheme_dotted_list_combines_internal((void **) v_at(predata, 0),
                                             v_size(predata),
                                             post_data);
    v_free(v);
    return obj;
}

void *scheme_quoted_datum_combines(Vector *v) {
    assert(v_size(v) == 2);
    SchemeObject *o_cdr = *(SchemeObject **) v_at(v, 1);
    SchemeObject *quote = (SchemeObject *) malloc(sizeof(SchemeObject));
    quote->_type = SCHEME_SYMBOL;
    quote->_data._symbol._value = strdup("quote");

    SchemeObject *p = (SchemeObject *) malloc(sizeof(SchemeObject));
    p->_type = SCHEME_PAIR;
    set_car(p, quote);
    set_cdr(p, o_cdr);
    v_free(v);
    return p;
}

void *scheme_quasi_quoted_datum_combines(Vector *v) {
    assert(v_size(v) == 2);
    SchemeObject *o_cdr = *(SchemeObject **) v_at(v, 1);
    SchemeObject *quasiquote = (SchemeObject *) malloc(sizeof(SchemeObject));
    quasiquote->_type = SCHEME_SYMBOL;
    quasiquote->_data._symbol._value = strdup("quasiquote");

    SchemeObject *p = (SchemeObject *) malloc(sizeof(SchemeObject));
    p->_type = SCHEME_PAIR;
    set_car(p, quasiquote);
    set_cdr(p, o_cdr);
    v_free(v);
    return p;
}

void *scheme_unquoted_datum_combines(Vector *v) {
    assert(v_size(v) == 2);
    SchemeObject *o_cdr = *(SchemeObject **) v_at(v, 1);
    SchemeObject *unquote = (SchemeObject *) malloc(sizeof(SchemeObject));
    unquote->_type = SCHEME_SYMBOL;
    unquote->_data._symbol._value = strdup("unquote");

    SchemeObject *p = (SchemeObject *) malloc(sizeof(SchemeObject));
    p->_type = SCHEME_PAIR;
    set_car(p, unquote);
    set_cdr(p, o_cdr);
    v_free(v);
    return p;
}

void *scheme_unquote_spliced_datum_combines(Vector *v) {
    assert(v_size(v) == 3);
    SchemeObject *o_cdr = *(SchemeObject **) v_at(v, 2);
    SchemeObject *unquote_splicing = (SchemeObject *) malloc(sizeof(SchemeObject));
    unquote_splicing->_type = SCHEME_SYMBOL;
    unquote_splicing->_data._symbol._value = strdup("unquote-splicing");

    SchemeObject *p = (SchemeObject *) malloc(sizeof(SchemeObject));
    p->_type = SCHEME_PAIR;
    set_car(p, unquote_splicing);
    set_cdr(p, o_cdr);
    v_free(v);
    return p;
}

void *scheme_vector_combines(Vector *v) {
    assert(v_size(v) == 3);

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    assert(obj != NULL);
    obj->_type = SCHEME_VECTOR;
    obj->_data._vector._value = *(Vector **) v_at(v, 1);
    obj->_data._vector._value->_cleanup_fn = wrapped_scheme_object_free;

    v_free(v);
    return obj;
}

void *scheme_character_emits(ParserEnv *pe,
                             Vector *tokens, void *binding_tree,
                             ParserCombinator *self) {
    char *s = (char *) match_category_emits(pe, tokens, binding_tree, self);
    char c;
    if (strcmp(s, "#\\newline") == 0) {
        c = '\n';
    } else if (strcmp(s, "#\\space") == 0) {
        c = ' ';
    } else {
        c = s[2];
    }

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_CHARACTER;
    obj->_data._character._value = c;
    return obj;
}

void *null_emits(__attribute__((unused)) ParserEnv *pe,
                 __attribute__((unused)) Vector *tokens,
                 __attribute__((unused)) void *binding_tree,
                 __attribute__((unused)) ParserCombinator *self) {
    return NULL;
}

void *scheme_symbol_emits(ParserEnv *pe,
                          Vector *tokens, void *binding_tree,
                          ParserCombinator *self) {
    char *id = strdup((char *) match_category_emits(pe, tokens, binding_tree, self));

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_SYMBOL;
    obj->_data._symbol._value = id;
    return obj;
}

void *scheme_boolean_emits(ParserEnv *pe,
                           Vector *tokens, void *binding_tree,
                           ParserCombinator *self) {
    char *s = (char *) match_category_emits(pe, tokens, binding_tree, self);

    bool b;
    if (strcmp(s, "#f") == 0) {
        b = false;
    } else {
        b = true;
    }

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_BOOLEAN;
    obj->_data._boolean._value = b;
    return obj;
}

void *scheme_string_emits(ParserEnv *pe,
                          Vector *tokens, void *binding_tree,
                          ParserCombinator *self) {
    char *s = (char *) match_category_emits(pe, tokens, binding_tree, self);
    size_t len = strlen(s);
    char *n = (char *) malloc(len - 1);
    len -= 2;
    n[len] = '\0';
    memcpy(n, ((char *)s + 1), len);

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    obj->_type = SCHEME_STRING;
    obj->_data._string._value = n;
    return obj;
}

void *scheme_number_emits(ParserEnv *pe,
                          Vector *tokens, void *binding_tree,
                          ParserCombinator *self) {
    const char *s = (const char *) match_category_emits(pe, tokens, binding_tree, self);

    SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    Bignum *bn = bn_make(strlen(s)); // a reasonable guess

    bn_set_from_string(bn, s);

    obj->_type = SCHEME_NUMBER;
    obj->_data._number._value = bn;
    return obj;
}

void add_primitives_to_symbol_table(Map *symbol_table);

bool scheme_is_special_form_symbol(SchemeEnv *se, SchemeObject *obj) {
    if (obj->_type != SCHEME_SYMBOL) return false;
    return m_get(se->_special_form_table, obj->_data._symbol._value) != NULL;
}

SchemeObject *scheme_eval(SchemeEnv *se, SchemeObject *form);

SchemeObject *scheme_and_special_form(SchemeEnv *se,
                                     SchemeObject *args) {
    size_t n_args = scheme_length(args);
    if (n_args == 0) {
        SchemeObject *true_obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        true_obj->_type = SCHEME_BOOLEAN;
        true_obj->_data._boolean._value = true;
        return true_obj;
    } else if (n_args == 1) {
        return scheme_eval(se, car(args));
    } else {
        SchemeObject *evaled = scheme_eval(se, car(args));
        if (!scheme_truthy(evaled))
            return evaled;
        return scheme_and_special_form(se, cdr(args));
    }
}

SchemeObject *scheme_or_special_form(SchemeEnv *se,
                                     SchemeObject *args) {
    size_t n_args = scheme_length(args);
    if (n_args == 0) {
        SchemeObject *false_obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        false_obj->_type = SCHEME_BOOLEAN;
        false_obj->_data._boolean._value = false;
        return false_obj;
    } else if (n_args == 1) {
        return scheme_eval(se, car(args));
    } else {
        SchemeObject *evaled = scheme_eval(se, car(args));
        if (scheme_truthy(evaled))
            return evaled;
        return scheme_or_special_form(se, cdr(args));
    }
}

SchemeObject *scheme_quote_special_form(__attribute__((unused)) SchemeEnv *se,
                                        SchemeObject *args) {
    return args;
}

SchemeObject *scheme_cond_evaluate_clause(SchemeEnv *se,
                                          SchemeObject *clause,
                                          bool *truthy, bool allow_else) {
    size_t length_clause = scheme_length(clause);
    if (car(clause)->_type == SCHEME_SYMBOL &&
        strcmp(car(clause)->_data._symbol._value, "else") == 0) {
        if (!allow_else) {
            printf("else encountered out of tail position in a cond special form.\n");
            assert(0);
        }
        *truthy = true;
    } else {
        SchemeObject *e_cond = scheme_eval(se, car(clause));
        if (e_cond == NULL) return e_cond;
        *truthy = scheme_truthy(e_cond);
        if (length_clause == 1) return e_cond;
    }
    if (*truthy == false) return NULL;

    assert(length_clause >= 2);
    SchemeObject *exprs = cdr(clause);
    while (true) {
        SchemeObject *e_expr = scheme_eval(se, car(exprs));
        exprs = cdr(exprs);
        if (exprs->_type == SCHEME_EMPTY_LIST) return e_expr;
    }
}

SchemeObject *scheme_cond_special_form(SchemeEnv *se,
                                       SchemeObject *args) {
    size_t n_args = scheme_length(args);
    bool truthy = false;
    bool allow_else_clause = false;
    if (n_args == 0) return NULL; // unspecified in the language
    if (n_args == 1) allow_else_clause = true;

    SchemeObject *resolution =
        scheme_cond_evaluate_clause(se, car(args), &truthy, true);
    if (truthy) return resolution;
    return scheme_cond_special_form(se, cdr(args));
}

char *scheme_generate_lambda_name(SchemeEnv *se) {
    size_t buffer_size = 64;
    char buffer[buffer_size];
    snprintf(buffer, buffer_size, "%zu", se->_n_lambdas);
    se->_n_lambdas++;
    return strdup(buffer);
}

void scheme_lambda_compile_parameters(SchemeObject *proc,
                                      SchemeObject *args) {
    proc->_data._compound_procedure._req_args = v_make(sizeof(SchemeObject *));
    proc->_data._compound_procedure._opt_args = v_make(sizeof(SchemeObject *));
    Vector *req_args = proc->_data._compound_procedure._req_args;
    Vector *opt_args = proc->_data._compound_procedure._opt_args;
    proc->_data._compound_procedure._rest = NULL;
    Vector *appending = req_args;
    bool in_optionals = false;
    while (args->_type != SCHEME_EMPTY_LIST) {
        SchemeObject *ec = car(args);
        if (ec->_type == SCHEME_SYMBOL &&
            strcmp(ec->_data._symbol._value, "#!optional") == 0) {
            in_optionals = true;
            appending = opt_args;
        } else if (ec->_type == SCHEME_SYMBOL &&
                   strcmp(ec->_data._symbol._value, "#!rest") == 0) {
            args = cdr(args);
            assert(scheme_length(args) == 1);
            proc->_data._compound_procedure._rest = car(args);
            return;
        } else {
            v_push_back(appending, &ec);
        }
        args = cdr(args);
    }
}

SchemeObject *scheme_lambda_special_form(SchemeEnv *se,
                                         SchemeObject *args) {
    size_t n_args = scheme_length(args);
    assert(n_args >= 2);

    SchemeObject *new_proc = (SchemeObject *) malloc(sizeof(SchemeObject));

    new_proc->_type = SCHEME_COMPOUND_PROCEDURE;
    scheme_lambda_compile_parameters(new_proc, car(args));
    new_proc->_data._compound_procedure._body = cdr(args);
    new_proc->_data._compound_procedure._n_args_expected = 0;
    new_proc->_data._compound_procedure._env = NULL; // for now
    new_proc->_data._compound_procedure._proc_name =
        scheme_generate_lambda_name(se);
    return new_proc;
}

SchemeObject *scheme_if_special_form(SchemeEnv *se,
                                     SchemeObject *args) {
    size_t n_args = scheme_length(args);
    SchemeObject *e_fst = scheme_eval(se, car(args));
    if (scheme_truthy(e_fst)) {
        return scheme_eval(se, car(cdr(args)));
    } else if (n_args == 3) {
        return scheme_eval(se, car(cdr(cdr(args))));
    }
    return NULL;
}

SchemeObject *scheme_define_special_form(SchemeEnv *se,
                                         SchemeObject *args) {
    size_t n_args = scheme_length(args);
    assert(n_args == 2); // not quite true... good enough for now
    assert(car(args)->_type == SCHEME_SYMBOL);
    SchemeObject *value = scheme_eval(se, car(cdr(args)));

    m_insert(se->_symbol_table, car(args)->_data._symbol._value, &value);
    return NULL;
}

void initialize_special_form_table(Map *special_form_table) {
    SchemeSpecialForm procedure = scheme_if_special_form;
    m_insert(special_form_table, "if", &procedure);

    procedure = scheme_or_special_form;
    m_insert(special_form_table, "or", &procedure);

    procedure = scheme_and_special_form;
    m_insert(special_form_table, "and", &procedure);

    procedure = scheme_cond_special_form;
    m_insert(special_form_table, "cond", &procedure);

    procedure = scheme_define_special_form;
    m_insert(special_form_table, "define", &procedure);

    procedure = scheme_lambda_special_form;
    m_insert(special_form_table, "lambda", &procedure);

    procedure = NULL;
    m_insert(special_form_table, "let", &procedure);
    m_insert(special_form_table, "begin", &procedure);

    procedure = scheme_quote_special_form;
    m_insert(special_form_table, "quote", &procedure);

    procedure = NULL;
    m_insert(special_form_table, "quasiquote", &procedure);
}

void scheme_push_lexical_environment(SchemeEnv *se, Map *lenv) {
    v_push_back(se->_lexical_environment_stack, &lenv);
}

void scheme_pop_lexical_environment(SchemeEnv *se) {
    assert(v_size(se->_lexical_environment_stack) >= 1);
    v_remove(se->_lexical_environment_stack,
             v_size(se->_lexical_environment_stack) - 1);
}

SchemeEnv *scheme_env_make() {
    SchemeEnv *se = (SchemeEnv *) malloc(sizeof(SchemeEnv));
    assert(se != NULL);

    se->_n_lambdas = 0;

    se->_lexical_environment_stack = v_make(sizeof(Map *));

    se->_symbol_table = m_make(sizeof(SchemeObject *));
    add_primitives_to_symbol_table(se->_symbol_table);
    se->_special_form_table = m_make(sizeof(SchemeObject *));
    initialize_special_form_table(se->_special_form_table);

    se->_lexer = lexer_make();
    lexer_add_category(se->_lexer, "IDENTIFIER");
    lexer_add_category(se->_lexer, "WHITESPACE");
    lexer_add_category(se->_lexer, "COMMENT");
    lexer_add_category(se->_lexer, "BOOLEAN");
    lexer_add_category(se->_lexer, "NUMBER");
    lexer_add_category(se->_lexer, "CHARACTER");
    lexer_add_category(se->_lexer, "STRING");
    lexer_add_category(se->_lexer, "OPEN_PAREN");
    lexer_add_category(se->_lexer, "CLOSE_PAREN");
    lexer_add_category(se->_lexer, "OPEN_VEC_PAREN");
    lexer_add_category(se->_lexer, "DOT");
    lexer_add_category(se->_lexer, "SINGLE_QUOTE");
    lexer_add_category(se->_lexer, "QUASI_QUOTE");
    lexer_add_category(se->_lexer, "UNQUOTE");
    lexer_add_category(se->_lexer, "AT");

    lexer_add_rule(se->_lexer, "WHITESPACE", "[ \t\n]+");
    lexer_add_rule(se->_lexer, "BOOLEAN", "#t");
    lexer_add_rule(se->_lexer, "BOOLEAN", "#f");
    lexer_add_rule(se->_lexer, "COMMENT", ";[^\n]*\n");
    lexer_add_rule(se->_lexer, "NUMBER", "0|-?[1-9][0-9]*");
    lexer_add_rule(se->_lexer, "IDENTIFIER", "\\+");
    lexer_add_rule(se->_lexer, "IDENTIFIER", "-");
    lexer_add_rule(se->_lexer, "IDENTIFIER", "\\.\\.\\.");
    lexer_add_rule(se->_lexer, "CHARACTER", "#\\\\newline");
    lexer_add_rule(se->_lexer, "CHARACTER", "#\\\\space");
    lexer_add_rule(se->_lexer, "CHARACTER", "#\\\\[:graph:]");
    //lexer_add_rule(se->_lexer, "STRING", "\"([^\"]|\\\")*\"");
    lexer_add_rule(se->_lexer, "STRING", "\\\"((\\\")|[^\\\"(\\\")])+\\\"");
    lexer_add_rule(se->_lexer, "OPEN_VEC_PAREN", "#\\(");
    lexer_add_rule(se->_lexer, "OPEN_PAREN", "\\(");
    lexer_add_rule(se->_lexer, "CLOSE_PAREN", "\\)");
    lexer_add_rule(se->_lexer, "DOT", "\\.");
    lexer_add_rule(se->_lexer, "SINGLE_QUOTE", "'");
    lexer_add_rule(se->_lexer, "QUASI_QUOTE", "`");
    lexer_add_rule(se->_lexer, "UNQUOTE", ",");
    lexer_add_rule(se->_lexer, "AT", "@");
    lexer_add_rule(se->_lexer, "IDENTIFIER",
                   "[a-zA-Z!\\$%&\\*/:<=>\\?~_\\^]"
                   "[a-zA-Z!\\$%&\\*/:<=>\\?~_\\^0-9\\.\\+-]*");

    se->_parser = parser_env_make();
    parser_env_add_parsers(se->_parser,
      atomic_parser("CHARACTER_ATOM_P", "CHARACTER", scheme_character_emits),
      atomic_parser("STRING_ATOM_P", "STRING", scheme_string_emits),
      atomic_parser("SYMBOL_ATOM_P", "IDENTIFIER", scheme_symbol_emits),
      atomic_parser("NUMBER_ATOM_P", "NUMBER", scheme_number_emits),
      atomic_parser("BOOLEAN_ATOM_P", "BOOLEAN", scheme_boolean_emits),
      atomic_parser("(_P", "OPEN_PAREN", null_emits),
      atomic_parser(")_P", "CLOSE_PAREN", null_emits),
      atomic_parser("#(_P", "OPEN_VEC_PAREN", null_emits),
      atomic_parser("DOT_ATOM_P", "DOT", null_emits),
      atomic_parser("QUOTE_ATOM_P", "SINGLE_QUOTE", null_emits),
      atomic_parser("QUASI_QUOTE_ATOM_P", "QUASI_QUOTE", null_emits),
      atomic_parser("UNQUOTE_ATOM_P", "UNQUOTE", null_emits),
      atomic_parser("AT_ATOM_P", "AT", null_emits),

      symbol_parser("LAMBDA_SYMBOL_P", "lambda", scheme_symbol_emits),
      symbol_parser("IF_SYMBOL_P", "if", scheme_symbol_emits),
      symbol_parser("DEFINE_SYMBOL_P", "define", scheme_symbol_emits),
      symbol_parser("DEFINESYNTAX_SYMBOL_P", "define-syntax", scheme_symbol_emits),
      symbol_parser("COND_SYMBOL_P", "cond", scheme_symbol_emits),
      symbol_parser("CASE_SYMBOL_P", "case", scheme_symbol_emits),
      symbol_parser("ELSE_SYMBOL_P", "else", scheme_symbol_emits),
      symbol_parser("AND_SYMBOL_P", "and", scheme_symbol_emits),
      symbol_parser("OR_SYMBOL_P", "or", scheme_symbol_emits),
      symbol_parser("LET_SYMBOL_P", "let", scheme_symbol_emits),
      symbol_parser("LET*_SYMBOL_P", "let*", scheme_symbol_emits),
      symbol_parser("LETREC_SYMBOL_P", "letrec", scheme_symbol_emits),
      symbol_parser("DO_SYMBOL_P", "do", scheme_symbol_emits),
      symbol_parser("DELAY_SYMBOL_P", "delay", scheme_symbol_emits),
      symbol_parser("BEGIN_SYMBOL_P", "begin", scheme_symbol_emits),
      symbol_parser("SET_SYMBOL_P", "set!", scheme_symbol_emits),
      symbol_parser("LETSYNTAX_SYMBOL_P", "let-syntax", scheme_symbol_emits),
      symbol_parser("LETRECSYNTAX_SYMBOL_P", "letrec-syntax", scheme_symbol_emits),
      symbol_parser("=>_SYMBOL_P", "=>", scheme_symbol_emits),

      any_parser("DATUM_P",
                 "BOOLEAN_ATOM_P",
                 "NUMBER_ATOM_P",
                 "CHARACTER_ATOM_P",
                 "STRING_ATOM_P",
                 "SYMBOL_ATOM_P",
                 "LIST_P",
                 "VECTOR_P",
                 NULL),
      many0_parser("MANY0_DEFINITION_P", ID_combines, "DEFINITION_P"),
      many1_parser("MANY1_DEFINITION_P", ID_combines, "DEFINITION_P"),
      many0_parser("MANY0_ITERATION_SPEC_P", ID_combines, "ITERATION_SPEC_P"),
      many1_parser("MANY1_ITERATION_SPEC_P", ID_combines, "ITERATION_SPEC_P"),
      many0_parser("MANY0_CASE_CLAUSE_P", ID_combines, "CASE_CLAUSE_P"),
      many1_parser("MANY1_CASE_CLAUSE_P", ID_combines, "CASE_CLAUSE_P"),
      many0_parser("MANY0_BINDING_SPEC_P", ID_combines, "BINDING_SPEC_P"),
      many1_parser("MANY1_BINDING_SPEC_P", ID_combines, "BINDING_SPEC_P"),
      many0_parser("MANY0_DATUM_P", ID_combines, "DATUM_P"),
      many1_parser("MANY1_DATUM_P", ID_combines, "DATUM_P"),
      many0_parser("MANY0_SYMBOL_P", ID_combines, "SYMBOL_ATOM_P"),
      many1_parser("MANY1_SYMBOL_P", ID_combines, "SYMBOL_ATOM_P"),
      many0_parser("MANY0_EXPRESSION_P", ID_combines, "EXPRESSION_P"),
      many1_parser("MANY1_COND_CLAUSE_P", ID_combines,
                   "COND_CLAUSE_P"),
      many0_parser("MANY0_COND_CLAUSE_P", ID_combines,
                   "COND_CLAUSE_P"),
      many1_parser("MANY1_EXPRESSION_P", ID_combines, "EXPRESSION_P"),
      many1_parser("MANY1_SYMBOL_P", ID_combines, "SYMBOL_ATOM_P"),
      seq_parser("VECTOR_P", scheme_vector_combines,
                 "#(_P",
                 "MANY0_DATUM_P",
                 ")_P",
                 NULL),
      seq_parser("SIMPLE_LIST_P",
                 scheme_simple_list_combines,
                 "(_P",
                 "MANY0_DATUM_P",
                 ")_P",
                 NULL),
      seq_parser("DOTTED_LIST_P",
                 scheme_dotted_list_combines,
                 "(_P",
                 "MANY1_DATUM_P",
                 "DOT_ATOM_P",
                 "DATUM_P",
                 ")_P",
                 NULL),
      seq_parser("QUOTE_DATUM_P",
                 scheme_quoted_datum_combines,
                 "QUOTE_ATOM_P",
                 "DATUM_P",
                 NULL),
      seq_parser("QUASI_QUOTE_DATUM_P",
                 scheme_quasi_quoted_datum_combines,
                 "QUASI_QUOTE_ATOM_P",
                 "DATUM_P",
                 NULL),
      seq_parser("UNQUOTE_DATUM_P",
                 scheme_unquoted_datum_combines,
                 "UNQUOTE_ATOM_P",
                 "DATUM_P",
                 NULL),
      seq_parser("UNQUOTE_SPLICING_DATUM_P",
                 scheme_unquote_spliced_datum_combines,
                 "UNQUOTE_ATOM_P",
                 "AT_ATOM_P",
                 "DATUM_P",
                 NULL),
      any_parser("ABBREVIATION_LIST_P",
                 "QUOTE_DATUM_P",
                 "QUASI_QUOTE_DATUM_P",
                 "UNQUOTE_DATUM_P",
                 "UNQUOTE_SPLICING_DATUM_P",
                 NULL),
      any_parser("LIST_P",
                 "SIMPLE_LIST_P",
                 "DOTTED_LIST_P",
                 "ABBREVIATION_LIST_P",
                 NULL),

      // data syntax now set up, parse scheme programs

      many0_parser("PROGRAM_P", ID_combines,
                   "FORM_P"),
      any_parser("FORM_P",
                 "DEFINITION_P",
                 "EXPRESSION_P",
                 NULL),
      any_parser("EXPRESSION_P",
                 "CONSTANT_P",
                 "SYMBOL_ATOM_P",
                 "QUOTE_DATUM_P",
                 "LAMBDA_P",
                 "IF_P",
                 "SET!_P",
                 "APPL_P",
                 //"LETSYNTAX_P",
                 //"LETRECSYNTAX_P",
                 "DERIVED_EXPRESSION_P",
                 NULL),
      any_parser("CONSTANT_P",
                 "NUMBER_ATOM_P",
                 "CHARACTER_ATOM_P",
                 "BOOLEAN_ATOM_P",
                 "STRING_ATOM_P",
                 NULL),
      any_parser("FORMALS_P",
                 "SYMBOL_ATOM_P",
                 "SIMPLE_LIST_SYMBOL_P",
                 "DOTTED_LIST_SYMBOL_P",
                 NULL),
      seq_parser("APPL_P",
                 scheme_simple_list_combines,
                 "(_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("SIMPLE_LIST_SYMBOL_P",
                 scheme_simple_list_combines,
                 "(_P",
                 "MANY0_SYMBOL_P",
                 ")_P",
                 NULL),
      seq_parser("DOTTED_LIST_SYMBOL_P",
                 scheme_dotted_list_combines,
                 "(_P",
                 "MANY1_SYMBOL_P",
                 "DOT_ATOM_P",
                 "MANY0_SYMBOL_P",
                 ")_P",
                 NULL),
      seq_parser("LAMBDA_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "LAMBDA_SYMBOL_P",
                 "FORMALS_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      any_parser("IF_P",
                 "IF_THEN_P",
                 "IF_THEN_ELSE_P",
                 NULL),
      seq_parser("SET!_P",
                 scheme_generic_combines,
                 "(_P",
                 "SET_SYMBOL_P",
                 "SYMBOL_ATOM_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("LETSYNTAX_P", // NOT IN USE
                 NULL, // NOT IN USE
                 "(_P",
                 "LETSYNTAX_SYMBOL_P",
                 "(_P",
                 "MANY0_SYNTAX_BINDING_P" // !
                 ")_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
     seq_parser("LETRECSYNTAX_P", // NOT IN USE
                 NULL, // NOT IN USE
                 "(_P",
                 "LETRECSYNTAX_SYMBOL_P",
                 "(_P",
                 "MANY0_SYNTAX_BINDING_P" // !
                 ")_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("IF_THEN_P",
                 scheme_generic_combines,
                 "(_P",
                 "IF_SYMBOL_P",
                 "EXPRESSION_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("IF_THEN_ELSE_P",
                 scheme_generic_combines, // !
                 "(_P",
                 "IF_SYMBOL_P",
                 "EXPRESSION_P",
                 "EXPRESSION_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      any_parser("DEFINITION_P",
                 "VARIABLE_DEFINITION_P",
                 //"SYNTAX_DEFINITION_P", // !
                 "BEGIN_DEFINITION_P", // !
                 //"LETSYNTAX_DEFINITION_P", // !
                 //"LETRECSYNTAX_DEFINITION_P", // !
                 //"DERIVED_DEFINITION_P", // !
                 NULL),
      any_parser("VARIABLE_DEFINITION_P",
                 "DEF_VAR_EXP_P",
                 "DEF_SYMBOL_LIST_BODY_P",
                 "DEF_DOTTED_SYMBOL_LIST_BODY_P",
                 NULL),
      seq_parser("BODY_P",
                 concat_combines,
                 "MANY0_DEFINITION_P",
                 "MANY1_EXPRESSION_P",
                 NULL),
      seq_parser("DEF_VAR_EXP_P",
                 scheme_generic_combines, //!
                 "(_P",
                 "DEFINE_SYMBOL_P",
                 "SYMBOL_ATOM_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("DEF_SYMBOL_LIST_BODY_P",
                 scheme_last_vec_combines, //!
                 "(_P",
                 "DEFINE_SYMBOL_P",
                 "SIMPLE_LIST_SYMBOL_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      seq_parser("DEF_DOTTED_SYMBOL_LIST_BODY_P",
                 scheme_last_vec_combines, //!
                 "(_P",
                 "DEFINE_SYMBOL_P",
                 "DOTTED_LIST_SYMBOL_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      seq_parser("BEGIN_DEFINITION_P",
                 scheme_last_vec_combines, // !
                 "(_P",
                 "BEGIN_SYMBOL_P",
                 "MANY0_DEFINITION_P",
                 ")_P",
                 NULL),
      seq_parser("SYNTAX_DEFINITION_P",
                 NULL, // !
                 "(_P",
                 // "DEFINESYNTAX_SYMBOL_P", // NOPE ! :D
                 "SYMBOL_ATOM_P",
                 // "TRANSFORMER_EXPRESSION_P", // NOPE! :D
                 ")_P",
                 NULL),
      any_parser("DERIVED_EXPRESSION_P",
                 "COND_P",
                 "CASE_P",
                 "AND_P",
                 "OR_P",
                 "LET_P",
                 "BEGIN_P",
                 "DO_P",
                 "DELAY_P",
                 "QUASI_QUOTE_DATUM_P",
                 NULL),
      any_parser("COND_P",
                 "COND_ELSE_P",
                 "COND_NELSE_P",
                 NULL),
      seq_parser("COND_ELSE_P",
                 scheme_cond_else_combines,
                 "(_P",
                 "COND_SYMBOL_P",
                 "MANY0_COND_CLAUSE_P",
                 "ELSE_P",
                 ")_P",
                 NULL),
      seq_parser("COND_NELSE_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "COND_SYMBOL_P",
                 "MANY1_COND_CLAUSE_P",
                 ")_P",
                 NULL),
      any_parser("COND_CLAUSE_P",
                 "COND_CLAUSE1_P",
                 "COND_CLAUSE2_P",
                 NULL),
      seq_parser("COND_CLAUSE1_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("COND_CLAUSE2_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "EXPRESSION_P",
                 "=>_SYMBOL_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      any_parser("CASE_P",
                 "CASE_NELSE_P",
                 "CASE_ELSE_P",
                 NULL),
      seq_parser("CASE_NELSE_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "CASE_SYMBOL_P",
                 "EXPRESSION_P",
                 "MANY1_CASE_CLAUSE_P",
                 ")_P",
                 NULL),
      seq_parser("ELSE_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "ELSE_SYMBOL_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("CASE_ELSE_P",
                 scheme_case_else_combines,
                 // (case expr <case_clause>* (else <expr>+))
                 "(_P",
                 "CASE_SYMBOL_P",
                 "EXPRESSION_P",
                 "MANY0_CASE_CLAUSE_P",
                 "ELSE_P",
                 ")_P",
                 NULL),
      seq_parser("AND_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "AND_SYMBOL_P",
                 "MANY0_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("OR_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "OR_SYMBOL_P",
                 "MANY0_EXPRESSION_P",
                 ")_P",
                 NULL),
      any_parser("LET_P",
                 "LET_BINDING_SPEC_P",
                 "LET_VARIABLE_BINDING_SPEC_P",
                 "LET*_P",
                 "LETREC_P",
                 NULL),
      seq_parser("SIMPLE_LIST_BINDING_SPEC_P",
                 scheme_generic_combines,
                 "(_P",
                 "MANY0_BINDING_SPEC_P",
                 ")_P",
                 NULL),
      seq_parser("SIMPLE_LIST_ITERATION_SPEC_P",
                 scheme_generic_combines,
                 "(_P",
                 "MANY0_ITERATION_SPEC_P",
                 ")_P",
                 NULL),
      seq_parser("BINDING_SPEC_P",
                 scheme_generic_combines,
                 "(_P",
                 "SYMBOL_ATOM_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("LET_BINDING_SPEC_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "LET_SYMBOL_P",
                 "SIMPLE_LIST_BINDING_SPEC_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      seq_parser("LET_VARIABLE_BINDING_SPEC_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "LET_SYMBOL_P",
                 "SYMBOL_ATOM_P",
                 "SIMPLE_LIST_BINDING_SPEC_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      seq_parser("LET*_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "LET*_SYMBOL_P",
                 "SIMPLE_LIST_BINDING_SPEC_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      seq_parser("LETREC_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "LETREC_SYMBOL_P",
                 "SIMPLE_LIST_BINDING_SPEC_P",
                 "BODY_P",
                 ")_P",
                 NULL),
      seq_parser("BEGIN_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "BEGIN_SYMBOL_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("DELAY_P",
                 scheme_generic_combines,
                 "(_P",
                 "DELAY_SYMBOL_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("CASE_CLAUSE_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "SIMPLE_LIST_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      any_parser("ITERATION_SPEC_P",
                 "ITERATION_SPEC1_P",
                 "ITERATION_SPEC2_P",
                 NULL),
      seq_parser("ITERATION_SPEC1_P",
                 scheme_generic_combines,
                 "(_P",
                 "SYMBOL_ATOM_P",
                 "EXPRESSION_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("ITERATION_SPEC2_P",
                 scheme_generic_combines,
                 "(_P",
                 "SYMBOL_ATOM_P",
                 "EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("DO_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "DO_SYMBOL_P",
                 "SIMPLE_LIST_ITERATION_SPEC_P",
                 "ATL2_LIST_EXPRESSION_P",
                 "MANY0_EXPRRESSION_P"
                 ")_P",
                 NULL),
      seq_parser("ATL2_LIST_EXPRESSION_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "EXPRESSION_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      NULL);
    return se;
}

void scheme_env_free(SchemeEnv *se) {
    lexer_free(se->_lexer);
    parser_env_free(se->_parser);
    m_free(se->_symbol_table);
    m_free(se->_special_form_table);
    v_free(se->_lexical_environment_stack);
    free(se);
}

bool scheme_self_evaluating(SchemeObject *form) {
    if (form->_type == SCHEME_NUMBER || form->_type == SCHEME_STRING ||
        form->_type == SCHEME_CHARACTER || form->_type == SCHEME_BOOLEAN ||
        form->_type == SCHEME_EMPTY_LIST ||
        form->_type == SCHEME_PRIMITIVE_PROCEDURE ||
        form->_type == SCHEME_COMPOUND_PROCEDURE) {
        return true;
    }
    return false;
}

typedef SchemeObject *(*SchemeTransforms)(SchemeEnv *, SchemeObject *);

SchemeObject *scheme_map(SchemeEnv *se, SchemeObject *list, SchemeTransforms f) {
    if (list->_type == SCHEME_EMPTY_LIST) {
        SchemeObject *new_list = (SchemeObject *) malloc(sizeof(SchemeObject));
        new_list->_type = SCHEME_EMPTY_LIST;
        return new_list;
    }
    SchemeObject *new_list = (SchemeObject *) malloc(sizeof(SchemeObject));
    new_list->_type = SCHEME_PAIR;
    SchemeObject *new_car = f(se, car(list));
    if (new_car == NULL) return NULL;
    set_car(new_list, new_car);
    set_cdr(new_list, scheme_map(se, cdr(list), f));
    return new_list;
}

SchemeObject *scheme_lexical_lookup(SchemeEnv *se,
                                    SchemeObject *symbol) {
    for (size_t idx = v_size(se->_lexical_environment_stack); idx --> 0;) {
        Map *lenv = *(Map **) v_at(se->_lexical_environment_stack, idx);
        SchemeObject **p_resolution = (SchemeObject **) m_get(lenv, symbol->_data._symbol._value);
        if (p_resolution) return *p_resolution;
    }
    return NULL;
}

SchemeObject *scheme_symbol_lookup(SchemeEnv *se,
                                   SchemeObject *symbol) {
    SchemeObject *lexical_resolution = scheme_lexical_lookup(se, symbol);
    if (lexical_resolution) return lexical_resolution;
    SchemeObject **p_resolution = (SchemeObject **)
        m_get(se->_symbol_table, symbol->_data._symbol._value);
    return p_resolution ? *p_resolution : NULL;
}

SchemeObject *scheme_apply_primitive(__attribute__((unused)) SchemeEnv *se,
                                     SchemeObject *f,
                                     SchemeObject *rest) {
    return f->_data._primitive_procedure._fn(rest);
}

void scheme_add_to_lexical_environment(Map *lenv, SchemeObject *pattern,
                                       SchemeObject **p_unbound_args) {
    if (pattern->_type == SCHEME_SYMBOL) {
        SchemeObject *to_be_bound = car(*p_unbound_args);
        m_insert(lenv, pattern->_data._symbol._value, &to_be_bound);
        *p_unbound_args = cdr(*p_unbound_args);
    } else {
        assert(pattern->_type == SCHEME_PAIR);
        if ((*p_unbound_args)->_type == SCHEME_EMPTY_LIST) {
            m_insert(lenv, car(pattern)->_data._symbol._value, car(cdr(pattern)));
            // do not attempt to modify p_unbound_args
        } else {
            SchemeObject *to_be_bound = car(*p_unbound_args);
            m_insert(lenv, car(pattern)->_data._symbol._value, &to_be_bound);
            *p_unbound_args = cdr(*p_unbound_args);
        }
    }
}

Map *scheme_build_lambda_lexical_environment(SchemeObject *proc,
                                             SchemeObject *unbound_args) {
    size_t n_unbound_args = scheme_length(unbound_args);
    Vector *req_args = proc->_data._compound_procedure._req_args;
    Vector *opt_args = proc->_data._compound_procedure._opt_args;
    if (n_unbound_args < v_size(req_args)) {
        printf("Insufficient required aruguments to ");
        scheme_object_fprint(stdout, proc);
        printf("At least %zu required, %zu found.\n",
               v_size(req_args), n_unbound_args);
        assert(0);
    }

    Map *lenv = m_make(sizeof(SchemeObject *));
    for (size_t v_idx = 0; v_idx < v_size(req_args); v_idx++) {
        scheme_add_to_lexical_environment(lenv, *(SchemeObject **)
                                          v_at(req_args, v_idx), &unbound_args);
    }
    for (size_t v_idx = 0; v_idx < v_size(opt_args); v_idx++) {
        scheme_add_to_lexical_environment(lenv, *(SchemeObject **)
                                          v_at(opt_args, v_idx), &unbound_args);
    }
    if (unbound_args->_type != SCHEME_EMPTY_LIST) {
        if (proc->_data._compound_procedure._rest == NULL) {
            printf("Too many arguments to ");
            scheme_object_fprint(stdout, proc);
            printf("Function does not expect #!rest parameters");
            assert(0);
        }
        SchemeObject *rest_obj = proc->_data._compound_procedure._rest;
        m_insert(lenv, rest_obj->_data._symbol._value, &unbound_args);
    }
    return lenv;
}

SchemeObject *scheme_apply_compound(__attribute__((unused)) SchemeEnv *se,
                                    __attribute__((unused)) SchemeObject *f,
                                    __attribute__((unused)) SchemeObject *rest) {
    Map *lenv = scheme_build_lambda_lexical_environment(f, rest);
    scheme_push_lexical_environment(se, lenv);
    SchemeObject *body = f->_data._compound_procedure._body;
    SchemeObject *evaled = NULL;
    while(body->_type != SCHEME_EMPTY_LIST) {
        evaled = scheme_eval(se, car(body));
        body = cdr(body);
    }
    scheme_pop_lexical_environment(se);
    m_free(lenv);
    return evaled;
}

SchemeObject *scheme_apply(__attribute__((unused)) SchemeEnv *se,
                           SchemeObject *e_f,
                           SchemeObject *e_rest) {
    if (e_f->_type == SCHEME_PRIMITIVE_PROCEDURE) {
        arity_check(e_f, e_rest);
        return scheme_apply_primitive(se, e_f, e_rest);
    } else if(e_f->_type == SCHEME_COMPOUND_PROCEDURE) {
        return scheme_apply_compound(se, e_f, e_rest);
    }

    assert(0);
    return NULL;
}

SchemeObject *scheme_eval_special_form(__attribute__((unused)) SchemeEnv *se,
                                       __attribute__((unused)) SchemeObject *special_form_symbol,
                                       __attribute__((unused)) SchemeObject *rest) {
    SchemeSpecialForm proc = *(SchemeSpecialForm *)
        m_get(se->_special_form_table, special_form_symbol->_data._symbol._value);

    if (proc == NULL) {
        printf("Don't know how to eval the special form %s.\n",
               special_form_symbol->_data._symbol._value);
        assert(0);
    }

    return proc(se, rest);
}

SchemeObject *scheme_eval(SchemeEnv *se,
                          SchemeObject *form) {
    if (scheme_self_evaluating(form)) return form;
    if (form->_type == SCHEME_SYMBOL) {
        // lookup symbol in the environment
        SchemeObject *resolution = scheme_symbol_lookup(se, form);
        if (resolution == NULL) {
            printf("Unable to resolve symbol: %s in this context.",
                   form->_data._symbol._value);
        }
        return resolution;
    }

    if (form->_type == SCHEME_VECTOR) {
        // evaluate each element of the vector
        SchemeObject *new_vec = (SchemeObject *) malloc(sizeof(SchemeObject));
        new_vec->_type = SCHEME_VECTOR;
        new_vec->_data._vector._value->_cleanup_fn = wrapped_scheme_object_free;

        SchemeObject *evaled_elem = NULL;
        for (size_t idx = 0; idx < v_size(form->_data._vector._value); idx++) {
            evaled_elem = scheme_eval(se, *(SchemeObject **)
                                      v_at(form->_data._vector._value, idx));
            if (evaled_elem == NULL) return NULL;
            v_push_back(new_vec->_data._vector._value, &evaled_elem);
        }
        return form;
    }

    if (form->_type == SCHEME_PAIR) {
        if (scheme_is_special_form_symbol(se, car(form))) {
            return scheme_eval_special_form(se, car(form), cdr(form));
        }
        SchemeObject *e_f = scheme_eval(se, car(form));
        if (e_f == NULL) return NULL;
        if (scheme_is_special_form_symbol(se, e_f)) {
            return scheme_eval_special_form(se, e_f, cdr(form));
        }

        // map eval rest
        SchemeObject *e_rest = scheme_map(se, cdr(form), scheme_eval);
        return scheme_apply(se, e_f, e_rest);
    }

    assert(0);
    return form;
}

void arity_error(const char *fn_name, size_t expected, size_t found) {
    printf("Arity Error: Function: %s expected %zu arguments but received %zu.\n",
           fn_name, expected, found);
    assert(0);
}

void arity_check(SchemeObject *f, SchemeObject *obj) {
    size_t expected;
    char *proc_name;
    size_t length = scheme_length(obj);
    if (f->_type == SCHEME_PRIMITIVE_PROCEDURE) {
        expected = f->_data._primitive_procedure._n_args_expected;
        proc_name = f->_data._primitive_procedure._proc_name;
    } else {
        expected = f->_data._compound_procedure._n_args_expected;
        proc_name = f->_data._compound_procedure._proc_name;
    }

    if (length != expected) arity_error(proc_name, expected, length);
}

void type_error(const char *fn_name, const char *expected) {
    printf("Type Error: Function: %s expected an argument of type %s.\n",
           fn_name, expected);
    assert(0);
}

SchemeObject *scheme_length_proc (SchemeObject *args) {
    if (!scheme_is_list(car(args))) type_error("length", "list");
    size_t length = scheme_length(car(args));
    SchemeObject *length_obj = (SchemeObject *) malloc(sizeof(SchemeObject));
    length_obj->_type = SCHEME_NUMBER;
    length_obj->_data._number._value = bn_make(8);
    bn_set(length_obj->_data._number._value, length);
    return length_obj;
}

SchemeObject *scheme_not_proc (SchemeObject *obj) {
    SchemeObject *punned = scheme_pun_object_proc(car(obj));
    punned->_data._boolean._value = !punned->_data._boolean._value;
    return punned;
}

void add_primitives_to_symbol_table(Map *symbol_table) {
    SchemeObject *new_symbol = (SchemeObject *) malloc(sizeof(SchemeObject));
    new_symbol->_type = SCHEME_PRIMITIVE_PROCEDURE;
    new_symbol->_data._primitive_procedure._fn = scheme_not_proc;
    new_symbol->_data._primitive_procedure._n_args_expected = 1;
    new_symbol->_data._primitive_procedure._proc_name = strdup("not");
    m_insert(symbol_table, "not", &new_symbol);

    new_symbol = (SchemeObject *) malloc(sizeof(SchemeObject));
    new_symbol->_type = SCHEME_PRIMITIVE_PROCEDURE;
    new_symbol->_data._primitive_procedure._fn = scheme_length_proc;
    new_symbol->_data._primitive_procedure._n_args_expected = 1;
    new_symbol->_data._primitive_procedure._proc_name = strdup("length");
    m_insert(symbol_table, "length", &new_symbol);
}

#endif
