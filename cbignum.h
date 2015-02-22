#ifndef CBIGNUM_H
#define CBIGNUM_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define BIGNUM_DEFAULT_SIZE 64

typedef struct {
    char *_digits;
    bool _negative;
    size_t _length;
    size_t _capacity;
} Bignum;

Bignum *bn_make(size_t size_hint) {
    Bignum *bn = (Bignum *) malloc(sizeof(Bignum));
    assert(bn != NULL);

    size_t allocated_digits = (size_hint != 0) ? size_hint : BIGNUM_DEFAULT_SIZE;
    bn->_capacity = allocated_digits;
    bn->_digits = (char *) malloc(allocated_digits);
    assert(bn->_digits != NULL);

    bn->_negative = false;
    bn->_length = 1;

    return bn;
}

void bn_free(Bignum *bn) {
    free(bn->_digits);
    free(bn);
}

void bn_fprint(FILE *f, Bignum *bn) {
    if (bn->_negative) fprintf(f, "-");
    for (size_t idx = bn->_length; idx --> 0;) {
        fprintf(f, "%c", '0' + bn->_digits[idx]);
    }
}

void bn_ensure_space(Bignum *bn) {
    if (bn->_length > bn->_capacity) {
        size_t resize_target = bn->_capacity * 2;
        resize_target = resize_target > BIGNUM_DEFAULT_SIZE ?
            resize_target : BIGNUM_DEFAULT_SIZE;
        bn->_capacity = resize_target;
        bn->_digits = (char *) realloc(bn->_digits, resize_target);
        assert(bn->_digits != NULL);
    }
}

void bn_set_from_string(Bignum *bn, const char *n) {
    if (n[0] == '-') {
        bn->_negative = true;
        n++;
    } else if (n[0] == '+') {
        bn->_negative = false;
        n++;
    }

    size_t len = strlen(n);

    bn->_length = len;
    bn->_digits[0] = 0;

    for (size_t idx = 0; idx < len; idx++) {
        bn->_digits[idx] = n[len - idx - 1] - '0';
    }
}

void bn_set(Bignum *bn, long long n) {
    bn->_negative = (n < 0);
    n = (n < 0) ? -n : n;

    bn->_length = 1;
    assert(bn->_capacity != 0);
    bn->_digits[0] = 0;
    char digit = 0;
    while (n) {
        bn_ensure_space(bn);

        digit = n % 10;
        bn->_digits[bn->_length - 1] = digit;
        n -= digit;
        n /= 10;
        bn->_length += 1;
    }
    bn->_length--;
    if (bn->_length == 0) bn->_length++;
}

void bn_negate(Bignum *bn) {
    bn->_negative = !bn->_negative;
}

void bn_copy(Bignum *bn_dst, Bignum *bn_src) {
    if (bn_dst->_capacity < bn_src->_length) {
        bn_dst->_digits = (char *) realloc(bn_dst->_digits, bn_src->_capacity);
        bn_dst->_capacity = bn_src->_capacity;
        assert(bn_dst->_digits != NULL);
    }

    bn_dst->_length = bn_src->_length;
    bn_dst->_negative = bn_src->_negative;
    memcpy(bn_dst->_digits, bn_src->_digits, bn_src->_length);
}

Bignum *bn_deepcopy(Bignum *bn) {
    Bignum *nbn = bn_make(bn->_capacity);
    nbn->_length = bn->_length;
    nbn->_negative = bn->_negative;
    nbn->_capacity = bn->_capacity;
    memcpy(nbn->_digits, bn->_digits, bn->_length);
    return nbn;
}

#endif
