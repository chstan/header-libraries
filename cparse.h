#ifndef CPARSE_H
#define CPARSE_H

#include <string.h>
#include <stdarg.h>

#include "csys.h"
#include "clex.h"
#include "cnarytree.h"
#include "cvector.h"
#include "cmap.h"

typedef struct {
    size_t _start;
    size_t _end;
    size_t _aux;
} ParserBinding;

typedef struct {
    Map *_parser_map;
    bool _strict;
} ParserEnv;

struct ParserCombinator;

typedef void *(*BindsFn)(ParserEnv *, Vector *, size_t, struct ParserCombinator *);
typedef void *(*EmitterFn)(ParserEnv *, Vector *, void *, struct ParserCombinator *);
typedef void *(*CombinerFn)(Vector *);

typedef struct ParserCombinator {
    char *_parser_label;

    BindsFn _bind;
    EmitterFn _emit;
    CombinerFn _combine;

    Vector *_aux;
} ParserCombinator;

void *ID_combines(Vector *v) {
    return v;
}

void *concat_combines(Vector *v) {
    Vector *vv = v_make(sizeof(void *));
    for (size_t v_idx = 0; v_idx < v_size(v); v_idx++) {
        v_append(vv, *(Vector **) v_at(v, v_idx));
        v_free(*(Vector **) v_at(v, v_idx));
    }
    v_free(v);
    return vv;
}

void *match_category_binds(__attribute__((unused)) ParserEnv *pe,
                           Vector *tokens, size_t start, ParserCombinator *self) {
    Vector *aux = self->_aux;
    if (start == v_size(tokens)) return NULL;
    char *category = *(char **) v_at(aux, 0);

    Token *token = (Token *) v_at(tokens, start);
    if (strcmp(category, token->_category->_category_label) == 0) {
        // matched
        void *cell = nary_cell_make(sizeof(ParserBinding));
        ParserBinding *binding = (ParserBinding *) nary_data(cell);
        binding->_start = start;
        binding->_end = start + 1;
        return cell;
    }
    return NULL;
}

void *match_exact_symbol_binds(__attribute__((unused)) ParserEnv *pe,
                               Vector *tokens, size_t start, ParserCombinator *self) {
    Vector *aux = self->_aux;
    if (start == v_size(tokens)) return NULL;
    char *symbol = *(char **) v_at(aux, 0);

    Token *token = (Token *) v_at(tokens, start);
    if (strcmp("IDENTIFIER", token->_category->_category_label) == 0
        && strcmp(symbol, token->_value) == 0) {
        // matched
        void *cell = nary_cell_make(sizeof(ParserBinding));
        ParserBinding *binding = (ParserBinding *) nary_data(cell);
        binding->_start = start;
        binding->_end = start + 1;
        return cell;
    }
    return NULL;
}

void *match_category_emits(__attribute__((unused)) ParserEnv *pe,
                           Vector *tokens, void *binding_tree,
                           __attribute__((unused)) ParserCombinator *self) {
    ParserBinding *binding = (ParserBinding *) nary_data(binding_tree);
    Token *token = (Token *) v_at(tokens, binding->_start);
    return token->_value;
}

void *any_binds(ParserEnv *pe, Vector *tokens, size_t start, ParserCombinator *self) {
    Vector *aux = self->_aux;
    void *cell = nary_cell_make(sizeof(ParserBinding));
    ParserBinding *binding = (ParserBinding *) nary_data(cell);
    binding->_start = start;

    for(size_t parser_idx = 0; parser_idx < v_size(aux); parser_idx++) {
        char *parser_label = *((char **) v_at(aux, parser_idx));
        ParserCombinator *parser =
            *((ParserCombinator **) m_get(pe->_parser_map, parser_label));
        assert(parser != NULL);
        void *subcell = parser->_bind(pe, tokens, start, parser);

        // found a match
        if (subcell != NULL) {
            ParserBinding *subbinding = (ParserBinding *) nary_data(subcell);
            binding->_end = subbinding->_end;
            nary_add_among_children(cell, subcell);

            // store succeeded parser
            binding->_aux = parser_idx;
            return cell;
        }
    }

    nary_free(cell, NULL);
    return NULL;
}

void *seq_binds(ParserEnv *pe, Vector *tokens, size_t start, ParserCombinator *self) {
    Vector *aux = self->_aux;
    void *cell = nary_cell_make(sizeof(ParserBinding));
    ParserBinding *binding = (ParserBinding *) nary_data(cell);
    binding->_start = start;
    for (size_t parser_idx = 0; parser_idx < v_size(aux); parser_idx++) {
        char *parser_label = *((char **) v_at(aux, parser_idx));
        ParserCombinator *parser =
            *((ParserCombinator **) m_get(pe->_parser_map, parser_label));
        assert(parser != NULL);
        void *subcell = parser->_bind(pe, tokens, start, parser);

        // failed to run a parser, quit
        if (subcell == NULL) {
            nary_free(cell, NULL);
            return NULL;
        }

        ParserBinding *subbinding = (ParserBinding *) nary_data(subcell);
        nary_add_among_children(cell, subcell);
        start = subbinding->_end;
    }
    binding->_end = start;
    return cell;
}

void *many0_binds(ParserEnv *pe, Vector *tokens, size_t start, ParserCombinator *self) {
    Vector *aux = self->_aux;
    void *cell = nary_cell_make(sizeof(ParserBinding));
    ParserBinding *binding = (ParserBinding *) nary_data(cell);
    binding->_start = start;
    char * parser_label = *((char **) v_at(aux, 0));
    ParserCombinator *parser =
        *((ParserCombinator **) m_get(pe->_parser_map, parser_label));
    assert(parser != NULL);

    while (true) {
        void *subcell = parser->_bind(pe, tokens, start, parser);

        // failed to run a parser, quit
        if (subcell == NULL) {
            break;
        }

        ParserBinding *subbinding = (ParserBinding *) nary_data(subcell);
        nary_add_among_children(cell, subcell);
        start = subbinding->_end;
    }

    binding->_end = start;
    return cell;
}

void *many1_binds(ParserEnv *pe, Vector *tokens, size_t start, ParserCombinator *self) {
    Vector *aux = self->_aux;
    void *cell = nary_cell_make(sizeof(ParserBinding));
    ParserBinding *binding = (ParserBinding *) nary_data(cell);
    binding->_start = start;
    char * parser_label = *((char **) v_at(aux, 0));
    ParserCombinator *parser =
        *((ParserCombinator **) m_get(pe->_parser_map, parser_label));
    assert(parser != NULL);

    size_t found = 0;
    while (true) {
        void *subcell = parser->_bind(pe, tokens, start, parser);

        // failed to run a parser, quit
        if (subcell == NULL) {
            break;
        }
        found++;
        ParserBinding *subbinding = (ParserBinding *) nary_data(subcell);
        nary_add_among_children(cell, subcell);
        start = subbinding->_end;
    }

    if (found == 0) {
        nary_free(cell, NULL);
        return NULL;
    }

    binding->_end = start;
    return cell;
}

void *any_emits(ParserEnv *pe, Vector *tokens, void *binding_tree, ParserCombinator *self) {
    Vector *aux = self->_aux;
    ParserBinding *binding = (ParserBinding *) nary_data(binding_tree);
    char *parser_label = *(char **) v_at(aux, binding->_aux);
    ParserCombinator *parser = *((ParserCombinator **)
                                 m_get(pe->_parser_map, parser_label));
    return parser->_emit(pe, tokens, *nary_child(binding_tree), parser);
}

void *seq_emits(ParserEnv *pe, Vector *tokens, void *binding_tree, ParserCombinator *self) {
    Vector *aux = self->_aux;
    Vector *v = v_make(sizeof(void *));
    void *data;
    void *subbinding_tree = *nary_child(binding_tree);
    for (size_t parser_idx = 0; parser_idx < v_size(aux); parser_idx++) {
        char *parser_label = *(char **) v_at(aux, parser_idx);
        ParserCombinator *parser = *((ParserCombinator **)
                                     m_get(pe->_parser_map, parser_label));
        assert(parser != NULL);
        assert(subbinding_tree != NULL);
        data = parser->_emit(pe, tokens, subbinding_tree, parser);
        v_push_back(v, &data);
        subbinding_tree = *nary_sibling(subbinding_tree);
    }
    void *combined = self->_combine(v); // up to combine to free if it wants to
    return combined;
}

void *many0_emits(ParserEnv *pe, Vector *tokens, void *binding_tree, ParserCombinator *self) {
    Vector *aux = self->_aux;
    void *subbinding_tree = *nary_child(binding_tree);
    void *data;

    Vector *v = v_make(sizeof(void *));

    char * parser_label = *((char **) v_at(aux, 0));
    ParserCombinator *parser =
        *((ParserCombinator **) m_get(pe->_parser_map, parser_label));
    assert(parser != NULL);

    while(subbinding_tree != NULL) {
        data = parser->_emit(pe, tokens, subbinding_tree, parser);
        v_push_back(v, &data);
        subbinding_tree = *nary_sibling(subbinding_tree);
    }

    void *combined = self->_combine(v);
    return combined;
}

ParserCombinator *seq_parser(const char *label, CombinerFn combines, ...) {
    ParserCombinator *p = (ParserCombinator *) malloc(sizeof(ParserCombinator));
    p->_parser_label = strdup(label);

    p->_aux = v_make(sizeof(char *));
    p->_aux->_cleanup_fn = vector_generic_free;

    va_list arg_list;
    va_start (arg_list, combines);

    while(true) {
        char *temp = va_arg(arg_list, char*);
        if (temp == NULL) break;
        char *subparser_label = strdup(temp);
        v_push_back(p->_aux, &subparser_label);
    }
    va_end(arg_list);

    p->_bind = seq_binds;
    p->_emit = seq_emits;
    p->_combine = combines;

    return p;
}

ParserCombinator *many0_parser(const char *label, CombinerFn combines, const char *subparser) {
    ParserCombinator *p = (ParserCombinator *) malloc(sizeof(ParserCombinator));
    p->_parser_label = strdup(label);

    char *subparser_dup = strdup(subparser);

    p->_aux = v_make(sizeof(char *));
    p->_aux->_cleanup_fn = vector_generic_free;


    v_push_back(p->_aux, &subparser_dup);

    p->_bind = many0_binds;
    p->_emit = many0_emits;
    p->_combine = combines;

    return p;
}

ParserCombinator *many1_parser(const char *label, CombinerFn combines, const char *subparser) {
    ParserCombinator *p = (ParserCombinator *) malloc(sizeof(ParserCombinator));
    p->_parser_label = strdup(label);

    char *subparser_dup = strdup(subparser);

    p->_aux = v_make(sizeof(char *));
    p->_aux->_cleanup_fn = vector_generic_free;


    v_push_back(p->_aux, &subparser_dup);

    p->_bind = many1_binds;
    p->_emit = many0_emits;
    p->_combine = combines;

    return p;
}

ParserCombinator *any_parser(const char *label, ...) {
    ParserCombinator *p = (ParserCombinator *) malloc(sizeof(ParserCombinator));
    p->_parser_label = strdup(label);

    p->_aux = v_make(sizeof(char *));
    p->_aux->_cleanup_fn = vector_generic_free;

    va_list arg_list;
    va_start (arg_list, label);

    while(true) {
        char *temp = va_arg(arg_list, char *);
        if (temp == NULL) break;
        char *subparser_label = strdup(temp);
        v_push_back(p->_aux, &subparser_label);
    }
    va_end(arg_list);

    p->_bind = any_binds;
    p->_emit = any_emits;

    return p;
}


ParserCombinator *atomic_parser(const char *label, const char *category,
                                EmitterFn emit) {
    ParserCombinator *p = (ParserCombinator *) malloc(sizeof(ParserCombinator));
    p->_parser_label = strdup(label);
    char *category_dup = strdup(category);

    p->_aux = v_make(sizeof(char *));
    p->_aux->_cleanup_fn = vector_generic_free;

    v_push_back(p->_aux, &category_dup);

    p->_bind = match_category_binds;
    p->_emit = emit;

    return p;
}

ParserCombinator *symbol_parser(const char *label, const char *symbol,
                                EmitterFn emit) {
    ParserCombinator *p = (ParserCombinator *) malloc(sizeof(ParserCombinator));
    p->_parser_label = strdup(label);
    char *symbol_dup = strdup(symbol);

    p->_aux = v_make(sizeof(char *));
    p->_aux->_cleanup_fn = vector_generic_free;

    v_push_back(p->_aux, &symbol_dup);

    p->_bind = match_exact_symbol_binds;
    p->_emit = emit;

    return p;
}

ParserCombinator *match_category_parser(const char *label, const char *category) {
    return atomic_parser(label, category, match_category_emits);
}

void parser_cleanup_fn(__attribute__((unused)) char *k,
                       void *p, __attribute__((unused)) void *aux) {
    ParserCombinator *t = *(ParserCombinator **) p;
    free(t->_parser_label);
    v_free(t->_aux);
    free(t);
}

ParserEnv *parser_env_make() {
    ParserEnv *pe = (ParserEnv *) malloc(sizeof(ParserEnv));
    assert(pe != NULL);
    pe->_strict = true;
    pe->_parser_map = m_make(sizeof(ParserCombinator *));
    pe->_parser_map->_cleanup_fn = parser_cleanup_fn;
    return pe;
}

void parser_env_add_parser(ParserEnv *pe, ParserCombinator *pc) {
    m_insert(pe->_parser_map, pc->_parser_label, &pc);
}

void parser_env_add_parsers(ParserEnv *pe, ...) {
    va_list arg_list;
    va_start (arg_list, pe);

    while(true) {
        ParserCombinator *parser = va_arg(arg_list, ParserCombinator *);
        if (parser == NULL) break;
        parser_env_add_parser(pe, parser);
    }
    va_end(arg_list);
}

void *parser_parse(ParserEnv *pe, const char* parser_label, Vector *tokens) {
    ParserCombinator *parser = *(ParserCombinator **) m_get(pe->_parser_map, parser_label);
    if (parser == NULL) return NULL;

    void *binding_tree = parser->_bind(pe, tokens, 0, parser);
    if (binding_tree == NULL) return NULL;

    if (pe->_strict) {
        // consume all tokens!
        ParserBinding *binding = (ParserBinding *) nary_data(binding_tree);
        if (binding->_end != v_size(tokens)) {
            nary_free(binding_tree, NULL);
            return NULL;
        }
    }
    void *emitted = parser->_emit(pe, tokens, binding_tree, parser);
    nary_free(binding_tree, NULL);
    return emitted;
}

void parser_env_free(ParserEnv *pe) {
    m_free(pe->_parser_map);
    free(pe);
}

#endif
