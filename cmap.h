#ifndef CMAP_H
#define CMAP_H
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define DEFAULT_BUCKET_COUNT 32
#define MIN_BUCKET_COUNT 8
#define REBALANCE_LOAD_FACTOR 2

typedef void (*MapMappableFn)(char *, void *, void*);

void map_generic_free(__attribute__((unused)) char *k, void *p,
                      __attribute__((unused)) void *aux) {
    free(*((void **) p));
}

typedef struct {
    size_t _length;
    size_t _stride;

    MapMappableFn _cleanup_fn;

    size_t _bucket_count;
    void *_buckets;
} Map;

void m_init(Map *m, size_t stride) {
    m->_cleanup_fn = NULL;
    m->_stride = stride;
    m->_length = 0;

    m->_bucket_count = DEFAULT_BUCKET_COUNT;
    m->_buckets = malloc(m->_bucket_count * sizeof(void *));
    assert(m->_buckets != NULL);
    for (size_t b_idx = 0; b_idx < m->_bucket_count; b_idx++) {
        ((void **)m->_buckets)[b_idx] = NULL;
    }
}

Map *m_make(size_t stride) {
    Map *m = (Map *) malloc(sizeof(Map));
    assert(m != NULL);

    m_init(m, stride);
    return m;
}

static size_t m_default_hash(const char *s, size_t n) {
    const unsigned long MULTIPLIER = 2630849305L; // magic prime number
    size_t hashcode = 0;
    for (size_t i = 0; s[i] != '\0'; i++)
        hashcode = hashcode * MULTIPLIER + s[i];
    return hashcode % n;
}

void *m_bucket_at(Map *m, size_t idx) {
    assert(idx < m->_bucket_count);
    return ((void **) m->_buckets) + idx;
}

size_t m_size(Map *m) {
    return m->_length;
}

void *m_value_from_elem(__attribute__((unused)) Map *m, void *elem) {
    return ((void **) elem) + 1;
}

char *m_key_from_elem(Map *m, void *elem) {
    return ((char *) elem) + (sizeof(char *) + m->_stride);
}

void m_set_bucket(Map *m, void *elem, size_t idx) {
    *(void **) m_bucket_at(m, idx) = elem;
}

void m_ll_map(Map *m, void *elem, MapMappableFn f, void *aux) {
    if (elem == NULL) return;

    f(m_key_from_elem(m, elem), m_value_from_elem(m, elem), aux);
    m_ll_map(m, *(void **) elem, f, aux);
}

void *m_ll_match(Map *m, void *elem, const char *k) {
    if (elem == NULL) return NULL;

    if (strcmp(m_key_from_elem(m, elem), k) == 0)
        return elem;

    return m_ll_match(m, *(void **) elem, k);
}

void *m_match_from_bucket(Map *m, size_t b_idx, const char *k) {
    void *fst = *(void **) m_bucket_at(m, b_idx);
    return m_ll_match(m, fst, k);
}

void *m_match(Map *m, const char *k) {
    size_t b_idx = m_default_hash(k, m->_bucket_count);
    return m_match_from_bucket(m, b_idx, k);
}

void *m_get(Map *m, const char *k) {
    void *matched_elem = m_match(m, k);
    return (void *) (matched_elem ? m_value_from_elem(m, matched_elem) : NULL);
}

void m_set_value_at_elem(Map *m, void *elem, void *data) {
    memcpy(m_value_from_elem(m, elem), data, m->_stride);
}

void *m_create_elem(Map *m, void *n_elem, const char *k, void *data) {
    void *elem = malloc(sizeof(char *) + (strlen(k) + 1)*sizeof(char)
                        + m->_stride);
    assert(elem != NULL);

    *(void **)elem = n_elem;
    strcpy(m_key_from_elem(m, elem), k);
    m_set_value_at_elem(m, elem, data);

    return elem;
}

void m_map(Map *m, MapMappableFn f, void *aux) {
    for (size_t b_idx = 0; b_idx < m->_bucket_count; b_idx++) {
        void *fst = *(void **) m_bucket_at(m, b_idx);
        m_ll_map(m, fst, f, aux);
    }
}

void m_ensure_space(Map *m) {
    if (m->_length < (m->_bucket_count * REBALANCE_LOAD_FACTOR))
        return;

    // allocate additional buckets
    void *new_buckets = calloc(m->_bucket_count * 2, sizeof(void *));
    assert(new_buckets);

    // migrate elements
    for (size_t b_old_idx = 0; b_old_idx < m->_bucket_count; b_old_idx++) {
        void *c_elem = *(void **) m_bucket_at(m, b_old_idx);
        void *n_elem = NULL;

        while (c_elem != NULL) {
            n_elem = *(void **) c_elem;

            size_t b_new_idx =
                m_default_hash(m_key_from_elem(m, c_elem), m->_bucket_count * 2);

            *(void **) c_elem = *((void **) new_buckets + b_new_idx);
            *((void **) new_buckets + b_new_idx) = c_elem;

            c_elem = n_elem;
        }
    }

    free(m->_buckets);
    m->_buckets = new_buckets;
    m->_bucket_count *= 2;
}

void m_remove(Map *m, const char *k) {
    size_t b_idx = m_default_hash(k, m->_bucket_count);

    void *l_elem = m_bucket_at(m, b_idx);
    if (!l_elem) return;

    void *c_elem = *(void **) l_elem;

    while (c_elem != NULL) {
        if (strcmp(m_key_from_elem(m, c_elem), k) == 0) {
            m->_length--;

            if (m->_cleanup_fn != NULL)
                m->_cleanup_fn(NULL, m_value_from_elem(m, c_elem), NULL);

            *(void **) l_elem = *(void **) c_elem;
            free(c_elem);
            return;
        }
        l_elem = c_elem;
        c_elem = *(void **) c_elem;
    }
}

void m_insert_unsafe(Map *m, const char *k, void *data) {
    m_ensure_space(m);

    size_t b_idx = m_default_hash(k, m->_bucket_count);
    void *elem = m_create_elem(m, *(void **) m_bucket_at(m, b_idx), k, data);

    m_set_bucket(m, elem, b_idx);
    m->_length++;
}

void m_insert(Map *m, const char *k, void *data) {
    void *elem = m_match(m, k);
    if (elem == NULL) {
        m_insert_unsafe(m, k, data);
        return;
    }

    if (m->_cleanup_fn != NULL)
        m->_cleanup_fn(NULL, m_value_from_elem(m, elem), NULL);

    m_set_value_at_elem(m, elem, data);
}

void m_free(Map *m) {
    for (size_t b_idx = 0; b_idx < m->_bucket_count; b_idx++) {
        void *c_elem = *(void **) m_bucket_at(m, b_idx);
        void *n_elem = NULL;
        while (c_elem != NULL) {
            n_elem = *(void **) c_elem;

            if (m->_cleanup_fn)
                m->_cleanup_fn(NULL, m_value_from_elem(m, c_elem), NULL);

            free(c_elem);
            c_elem = n_elem;
        }
    }

    free(m->_buckets);
    free(m);
}

#endif
