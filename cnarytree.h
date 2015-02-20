#ifndef CNARYTREE_H
#define CNARYTREE_H

#include <string.h>
#include <assert.h>
#include <stdlib.h>

typedef void (*NAryCleanupFn)(void *);
typedef void (*NAryMappableFn)(void *, void *);

void *nary_data(void *cell) {
    return ((void **) cell) + 2;
}

void **nary_child(void *cell) {
    return (void **) cell;
}

void **nary_sibling(void *cell) {
    return ((void **) cell) + 1;
}

void *nary_last_sibling(void *cell) {
    if (*nary_sibling(cell) == NULL) return cell;
    return nary_last_sibling(*(nary_sibling(cell)));
}

void nary_add_as_sibling(void *left_cell, void *new_sibling) {
    void *last_cell = nary_last_sibling(left_cell);
    *nary_sibling(last_cell) = new_sibling;
}

void *nary_cell_make(size_t stride) {
    void *cell = malloc(stride + (2 * sizeof(void *)));
    assert(cell != NULL);
    *nary_sibling(cell) = NULL;
    *nary_child(cell) = NULL;
    return cell;
}

void nary_map(void *cell, NAryMappableFn f, void *aux) {
    if (cell == NULL) return;

    f(nary_data(cell), aux);
    nary_map(*nary_child(cell), f, aux);
    nary_map(*nary_sibling(cell), f, aux);
}

void nary_free(void *cell, NAryCleanupFn f) {
    if (cell == NULL) return;

    if (f != NULL) f(nary_data(cell));
    nary_free(*nary_sibling(cell), f);
    nary_free(*nary_child(cell), f);
    free(cell);
}

void nary_free_children(void *cell, NAryCleanupFn f) {
    nary_free(*nary_child(cell), f);
    *nary_child(cell) = NULL;
}
void nary_add_as_child(void *left_cell, void *new_child, NAryCleanupFn f) {
    nary_free_children(left_cell, f);
    *nary_child(left_cell) = new_child;
}

void nary_add_among_children(void *parent_cell, void *new_child) {
    if (*nary_child(parent_cell) == NULL) {
        *nary_child(parent_cell) = new_child;
        return;
    }
    nary_add_as_sibling(*nary_child(parent_cell), new_child);
}

#endif
