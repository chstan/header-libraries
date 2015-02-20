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
    SCHEME_EMPTY_LIST,
    SCHEME_BOOLEAN,
    SCHEME_NUMBER,
    SCHEME_SYMBOL,
    SCHEME_CHARACTER,
    SCHEME_STRING,
    SCHEME_VECTOR,
    SCHEME_PAIR,
    SCHEME_PRIMITIVE_PROCEDURE,
    SCHEME_COMPOUND_PROCEDURE
} SchemeObjectType;

struct SchemeObject;
typedef struct SchemeObject *(*SchemeProcedure)(struct SchemeObject *);

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
        } _primitive_procedure;
        struct {
            struct SchemeObject *_parameters;
            struct SchemeObject *_body;
            struct SchemeObject *_env;
        } _compound_procedure;
    } _data;
} SchemeObject;

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

bool scheme_is_quote_variety_list(SchemeObject *o) {
    if (!scheme_is_list(o)) return false;
    if (car(o)->_type == SCHEME_SYMBOL) {
        char *s = car(o)->_data._symbol._value;
        if(strcmp(s, "quote") == 0) return true;
        if(strcmp(s, "quasiquote") == 0) return true;
        if(strcmp(s, "unquote") == 0) return true;
        if(strcmp(s, "unquote-splicing") == 0) return true;
    }
    return false;
}

void scheme_list_fprint(FILE *f, SchemeObject *o, bool parens);

void scheme_quoted_list_fprint(FILE *f, SchemeObject *o) {
    char *s = car(o)->_data._symbol._value;
    if(strcmp(s, "quote") == 0) fprintf(f, "'");
    if(strcmp(s, "quasiquote") == 0) fprintf(f, "`");
    if(strcmp(s, "unquote") == 0) fprintf(f, ",");
    if(strcmp(s, "unquote-splicing") == 0) fprintf(f, ",@");
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
        assert(0);
        break;
    case SCHEME_COMPOUND_PROCEDURE:
        assert(0);
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

typedef struct {
    LexerEnv *_lexer;
    ParserEnv *_parser;
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
                                                   void **cdr) {
    if (n == 1) {
        SchemeObject *obj = (SchemeObject *) malloc(sizeof(SchemeObject));
        obj->_type = SCHEME_PAIR;
        set_car(obj, *(SchemeObject **) cars);
        set_cdr(obj, *(SchemeObject **) cdr);
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
    Vector *postdata = *(Vector **) v_at(v, 3);
    assert(v_size(predata) >= 1);
    assert(v_size(postdata) == 1);
    SchemeObject *obj =
        scheme_dotted_list_combines_internal((void **) v_at(predata, 0),
                                             v_size(predata),
                                             (void **) v_at(postdata, 0));
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

SchemeEnv *scheme_env_make() {
    SchemeEnv *se = (SchemeEnv *) malloc(sizeof(SchemeEnv));
    assert(se != NULL);

    se->_lexer = lexer_make();
    lexer_add_category(se->_lexer, "IDENTIFIER");       // FIN
    lexer_add_category(se->_lexer, "WHITESPACE");       // FIN
    lexer_add_category(se->_lexer, "COMMENT");          // FIN
    lexer_add_category(se->_lexer, "BOOLEAN");          // FIN
    lexer_add_category(se->_lexer, "NUMBER");           // FIN
    lexer_add_category(se->_lexer, "CHARACTER");        // FIN
    lexer_add_category(se->_lexer, "STRING");           // FIN
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
    lexer_add_rule(se->_lexer, "STRING", "\"([^\"]|\\\")*\"");
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
                 "DOT_ATOM_P"
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
                 "COND_NELSE_P",
                 "COND_ELSE_P",
                 NULL),
      seq_parser("COND_ELSE_P",
                 scheme_cond_else_combines,
                 "(_P",
                 "COND_SYMBOL_P",
                 "MANY1_COND_CLAUSE_P",
                 "ELSE_P",
                 ")_P",
                 NULL),
      seq_parser("COND_NELSE_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "COND_SYMBOL_P",
                 "MANY1_COND_CLAUSE_P"
                 ")_P",
                 NULL),
      many1_parser("MANY1_COND_CLAUSE_P", ID_combines,
                   "COND_CLAUSE_P"),
      any_parser("COND_CLAUSE_P",
                 "COND_CLAUSE1_P",
                 "COND_CLAUSE2_P",
                 NULL),
      seq_parser("COND_CLAUSE1_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "COND_SYMBOL_P",
                 "MANY1_EXPRESSION_P",
                 ")_P",
                 NULL),
      seq_parser("COND_CLAUSE2_P",
                 scheme_last_vec_combines,
                 "(_P",
                 "COND_SYMBOL_P",
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
                 "CASE_SYMBOL_P"
                 "EXPRESSION_P"
                 "MANY1_CASE_CLAUSE_P"
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
                 "CASE_SYMBOL_P"
                 "EXPRESSION_P"
                 "MANY0_CASE_CLAUSE_P"
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
    free(se);
}

#endif
