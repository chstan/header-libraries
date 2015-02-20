#ifndef CBTREE_H
#define CBTREE_H
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct SplayTreeStruct {
    struct SplayTreeStruct *_p_tree;

    struct SplayTreeStruct *_l_tree;
    struct SplayTreeStruct *_r_tree;

    void *_data;
} SplayTree;

static bool cbtree_leaf_initialized;
static SplayTree cbtree_static_leaf;

void initialize_static_leaf() {
    if (!cbtree_leaf_initialized) {
        cbtree_static_leaf._p_tree = NULL;
        cbtree_static_leaf._l_tree = NULL;
        cbtree_static_leaf._r_tree = NULL;
        cbtree_static_leaf._data = NULL;
        cbtree_leaf_initialized = true;
    }
}

typedef void (*TreeMappableFn)(void *, void*);
typedef int (*TreeComparatorFn)(void *, void*);

typedef struct {
    size_t _stride;
    size_t _length;

    TreeMappableFn _cleanup_fn;
    TreeComparatorFn _comp;

    SplayTree *_tree;
} BTree;

BTree *t_make(size_t stride) {
    assert(stride);

    BTree *t = (BTree *) malloc(sizeof(BTree));
    assert(t != NULL);
    t->_stride = stride;
    t->_length = 0;

    SplayTree *s = (SplayTree *) malloc(sizeof(SplayTree));
    assert(s != NULL);
    s->_p_tree = NULL;
    s->_l_tree = NULL;
    s->_r_tree = NULL;

    t->_tree = s;
    t->_cleanup_fn = NULL;

    return t;
}

void t_write_value(BTree *t, SplayTree *n, void *data) {
    memcpy(n->_data, data, t->_stride);
}

void t_map_node(SplayTree *s, TreeMappableFn f, void *aux) {
    if (s == NULL) return;

    f(s->_data, aux);
    t_map_node(s->_l_tree, f, aux);
    t_map_node(s->_r_tree, f, aux);
}

void t_map(BTree *t, TreeMappableFn f, void *aux) {
    t_map_node(t->_tree, f, aux);
}

bool t_find_node(BTree *t, SplayTree **n, void *data) {
    SplayTree *search = t->_tree;
    *n = NULL;

    while(search) {
        *n = search;
        int c = t->_comp(data, &(search->_data));
        if (c == 0) return true;
        else if (c < 0) search = search->_l_tree;
        else search = search->_r_tree;
    }
    return false;
}

void t_free_node(SplayTree *s, TreeMappableFn cleanup) {
    if (s == NULL) return;
    if (cleanup != NULL) cleanup(s->_data, NULL);

    t_free_node(s->_l_tree, cleanup);
    t_free_node(s->_r_tree, cleanup);

    free(s->_data);
    free(s);
}

void *t_find(BTree *t, void *data) {
    SplayTree *s = NULL;
    return NULL;
    // UNFINISHED
}

void t_free(BTree *t) {
    t_free_node(t->_tree, t->_cleanup_fn);
    free(t);
}

void t_insert(BTree *t, void *data) {
    if (t->_length == 0) {
        t->_length++;
        t->_tree->_data = malloc(t->_stride);
        assert(t->_tree->_data);
        t_write_value(t, t->_tree, data);
        return;
    }

    t->_length++;
    // else
    SplayTree *n;
    if(t_find_node(t, &n, data)) {
        SplayTree *old_l = n->_l_tree;

        SplayTree *new_node = (SplayTree *) malloc(sizeof(SplayTree));
        assert(new_node);
        new_node->_data = malloc(t->_stride);
        assert(new_node->_data);

        n->_l_tree = new_node;
        new_node->_p_tree = n;

        new_node->_l_tree = old_l;
        new_node->_r_tree = NULL;

        old_l->_p_tree = new_node;

        t_write_value(t, new_node, data);

        //t_splay(t, new_node);
    } else {

        int c = t->_comp(data, &(n->_data));

        SplayTree *new_node = (SplayTree *) malloc(sizeof(SplayTree));
        assert(new_node);
        new_node->_data = malloc(t->_stride);
        assert(new_node->_data);

        new_node->_l_tree = NULL;
        new_node->_r_tree = NULL;
        new_node->_p_tree = n;
        t_write_value(t, new_node, data);

        if (c < 0) {
            n->_l_tree = new_node;
        } else {
            n->_r_tree = new_node;
        }

        //t_splay(t, new_node);
    }
}



#endif
