#ifndef CLEX_H
#define CLEX_H

#include <stdbool.h>
#include <regex.h>
#include <assert.h>

#include "cvector.h"

typedef struct {
    char * _category_label;
    size_t _category_id;
} TokenCategory;

typedef struct {
    regex_t _regex;
    TokenCategory *_category;
} LexerRule;

typedef struct {
    char *_value;
    TokenCategory *_category;
} Token;

typedef struct {
    Vector *_token_categories;
    Vector *_rules;

    bool _categories_initialized;
    bool _rules_initialized;
} LexerEnv;

void print_token(void *p, __attribute__((unused)) void *aux) {
    Token *t = (Token *) p;
    printf("%s\t|%s|\n", t->_category->_category_label, t->_value);
}

void rule_cleanup_fn(void *p, __attribute__((unused)) void *aux) {
    regfree(&((LexerRule *) p)->_regex);
}

void token_cleanup_fn(void *p, __attribute__((unused)) void *aux) {
    free(((Token *) p)->_value);
}

void token_category_cleanup_fn(void *p, __attribute__((unused)) void *aux) {
    free(((TokenCategory *) p)->_category_label);
}

LexerEnv *lexer_make() {
    LexerEnv *le = (LexerEnv *) malloc(sizeof(LexerEnv));
    le->_categories_initialized = false;
    le->_rules_initialized = false;
    le->_token_categories = v_make(sizeof(TokenCategory));
    le->_rules = v_make(sizeof(LexerRule));

    le->_rules->_cleanup_fn = rule_cleanup_fn;
    le->_token_categories->_cleanup_fn = token_category_cleanup_fn;
    return le;
}

void lexer_free(LexerEnv *le) {
    v_free(le->_token_categories);
    v_free(le->_rules);
    free(le);
}

void lexer_add_category(LexerEnv *le, const char *category_label) {
    assert(!le->_categories_initialized);

    TokenCategory tc;
    char *new_label = strdup(category_label);
    tc._category_label = new_label;
    tc._category_id = v_size(le->_token_categories);
    v_push_back(le->_token_categories, &tc);
}

bool token_category_matches_label(const void *p, const void *cat_label) {
    return (strcmp(((const TokenCategory *) p)->_category_label, (char *) cat_label) == 0);
}

void lexer_add_rule(LexerEnv *le, const char *category_label, const char *rule_pattern) {
    assert(!le->_rules_initialized);
    le->_categories_initialized = true;

    LexerRule rule;
    TokenCategory *tc = (TokenCategory *) v_find(le->_token_categories,
                                                 token_category_matches_label,
                                                 category_label);

    assert(tc != NULL);
    rule._category = tc;

    int comp_code = regcomp(&rule._regex, rule_pattern, REG_EXTENDED | REG_NEWLINE);
    if (comp_code != 0) {
        size_t error_buffer_size = 512;
        char *error_buffer = (char *) malloc(error_buffer_size + 1);
        assert(error_buffer != NULL);
        regerror(comp_code, &rule._regex, error_buffer, error_buffer_size);
        fprintf(stderr, "regcomp error: %s\n", error_buffer);
        free(error_buffer);
        assert(0);
    }

    v_push_back(le->_rules, &rule);
}

const char *lexer_rule_accepts(LexerRule *rule, const char *input, regmatch_t *matches) {
    if (regexec(&rule->_regex, input, 0, NULL, 0) == REG_NOMATCH) {
        return NULL;
    }
    int result = regexec(&rule->_regex, input, 1, matches, 0);
    if (result == REG_NOMATCH) {
        return NULL;
    }

    if (matches[0].rm_so != 0) {
        return NULL;
    }
    assert(matches[0].rm_so == 0);
    return input + matches[0].rm_eo;
}

const char *lexer_read_token(LexerEnv *le, const char *input, Token *t, regmatch_t *matches) {
    for (size_t rule_idx = 0; rule_idx < v_size(le->_rules); rule_idx++) {
        LexerRule *rule = (LexerRule *) v_at(le->_rules, rule_idx);
        const char *next = lexer_rule_accepts(rule, input, matches);
        if (next != NULL) {
            t->_category = rule->_category;
            size_t l = next - input;
            t->_value = (char *) malloc(l + 1);
            strncpy(t->_value, input, l);
            t->_value[l] = '\0';
            return next;
        }
    }
    return NULL;
}

Vector *lexer_lex(LexerEnv *le, const char *input) {
    le->_rules_initialized = true;
    regmatch_t matches[1];

    Vector *tokens = v_make(sizeof(Token));
    tokens->_cleanup_fn = token_cleanup_fn;

    Token t;
    while(input[0] != '\0') {
        input = lexer_read_token(le, input, &t, matches);
        if (input == NULL) {
            // could not lex further
            v_map(tokens, print_token, NULL);
            v_free(tokens);
            return NULL;
        }
        v_push_back(tokens, &t);
    }

    return tokens;
}

#endif
