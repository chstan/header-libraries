#ifndef CVECTOR_H
#define CVECTOR_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define DEFAULT_VECTOR_SIZE 16

typedef void (*VectorMappableFn)(void *, void *);
typedef bool (*VectorPredicateFn)(const void *, const void *);

typedef struct {
    void *_data;
    size_t _length;
    size_t _capacity;
    size_t _stride;

    VectorMappableFn _cleanup_fn;
} Vector;

void vector_generic_free(void *p, __attribute__((unused)) void *aux) {
    free(*((void **) p));
}

void vec_init(Vector *v, size_t stride) {
    assert(stride > 0);

    v->_stride = stride;
    v->_capacity = DEFAULT_VECTOR_SIZE;
    v->_data = malloc(v->_capacity * stride);
    assert(v->_data != NULL);
    v->_length = 0;
    v->_cleanup_fn = NULL;
}

Vector *v_make(size_t stride) {
    Vector *v = (Vector *) malloc(sizeof(Vector));
    assert(v != NULL);
    vec_init(v, stride);
    return v;
}

size_t v_size(const Vector *v) {
    return v->_length;
}

void *v_find(Vector *v, VectorPredicateFn pred, const void *aux) {
    char *current = (char *) v->_data;
    for (size_t idx = 0; idx < v_size(v); idx++) {
        if (pred((void *) current, aux))
            return current;
        current += (v->_stride);
    }
    return NULL;
}

void v_map(Vector *v, VectorMappableFn f, void *aux) {
    char *current = (char *) v->_data;
    for (size_t idx = 0; idx < v_size(v); idx++) {
        f((void *) current, aux);
        current += (v->_stride);
    }
}

void v_free(Vector *v) {
    if (v->_cleanup_fn)
        v_map(v, v->_cleanup_fn, NULL);
    free(v->_data);
    free(v);
}

void *v_at_unsafe(const Vector *v, size_t idx) {
    return (void *) (((char *) v->_data) + (idx * v->_stride));
}

void *v_at(const Vector *v, size_t idx) {
    assert(idx < v_size(v));
    return v_at_unsafe(v, idx);
}

void v_replace_at(Vector *v, void *data, size_t idx) {
    void *dest = v_at(v, idx);
    memcpy(dest, data, v->_stride);
}

void v_extend(Vector *v, size_t c) {
    void *new_data = realloc(v->_data, v->_stride * c);
    assert(v->_data != NULL);
    v->_data = new_data;
    v->_capacity = c;
}

void v_remove(Vector *v, size_t idx) {
    void *d = v_at(v, idx);
    memmove(d, ((char *) d) + v->_stride, v->_stride * (v_size(v) - idx - 1));
    v->_length--;
}

void v_push_back(Vector *v, void *data) {
    if (v_size(v) == v->_capacity) {
        v_extend(v, v->_capacity * 2);
    }
    memcpy(v_at_unsafe(v, v_size(v)), data, v->_stride);
    v->_length++;
}

Vector *v_filter(Vector *v, VectorPredicateFn pred, const void *aux) {
    Vector *o = v_make(v->_stride);
    o->_cleanup_fn = NULL; // explicitly!

    char *current = (char *) v->_data;
    for (size_t idx = 0; idx < v_size(v); idx++) {
        if (pred((void *) current, aux))
            v_push_back(o, current);
        current += (v->_stride);
    }

    return o;
}

void *v_end(Vector *v) {
    return v_at_unsafe(v, v_size(v));
}

void *v_start(Vector *v) {
    return v_at_unsafe(v, 0);
}

void v_append(Vector *f, Vector *s) {
    assert(f->_stride == s->_stride);
    if (f->_capacity < s->_length) v_extend(f, s->_length);
    memcpy(v_end(f), v_start(s), s->_stride * s->_length);
    f->_length += s->_length;
}

#endif
