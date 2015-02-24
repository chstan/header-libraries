#ifndef CBIGNUM_H
#define CBIGNUM_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define BIGNUM_DEFAULT_SIZE 32
#define BIGNUM_LEQ -1
#define BIGNUM_EQ 0
#define BIGNUM_GEQ 1

typedef struct {
    char *_digits;
    bool _negative;
    size_t _length;
    size_t _capacity;
} Bignum;

void bn_negate(Bignum *bn) {
    bn->_negative = !bn->_negative;
}

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


void bn_ensure_at_least(Bignum *bn, size_t n_digits) {
    if (n_digits > bn->_capacity) {
        size_t resize_target = bn->_capacity * 2;
        if (resize_target < n_digits) resize_target = n_digits;
        if (resize_target < BIGNUM_DEFAULT_SIZE) resize_target = BIGNUM_DEFAULT_SIZE;
        bn->_capacity = resize_target;
        bn->_digits = (char *) realloc(bn->_digits, resize_target);
        assert(bn->_digits != NULL);
    }
}

void bn_ensure_space(Bignum *bn) {
    bn_ensure_at_least(bn, bn->_length);
}


int bn_compare(Bignum *left, Bignum *right) {
    if (left->_negative && !right->_negative) return BIGNUM_LEQ;
    if (!left->_negative && right->_negative) return BIGNUM_GEQ;
    if (left->_negative && right->_negative) {
        bn_negate(left);
        bn_negate(right);
        size_t comp = bn_compare(left, right);
        bn_negate(left);
        bn_negate(right);
        return -comp;
    }

    if (right->_length > left->_length) return BIGNUM_LEQ;
    if (left->_length > right->_length) return BIGNUM_GEQ;

    for (size_t idx = left->_length; idx --> 0;) {
        int dc = left->_digits[idx] - right->_digits[idx];
        if (dc != 0) {
            return dc;
        }
    }
    return BIGNUM_EQ;
}

void bn_copy(Bignum *dst, const Bignum *src) {
    bn_ensure_at_least(dst, src->_length);
    dst->_length = src->_length;
    dst->_negative = src->_negative;
    memcpy(dst->_digits, src->_digits, src->_length);
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

char bn_char_at(Bignum *bn, size_t idx) {
    if (idx >= bn->_length) return 0;
    return bn->_digits[idx];
}

void bn_add_no_malloc(Bignum *bn_left, Bignum *bn_right,
                      Bignum *bn_result);
void bn_add_eq(Bignum *bn_left, Bignum *bn_right);
Bignum *bn_add(Bignum *bn_left, Bignum *bn_right);
void bn_subtract_no_malloc(Bignum *bn_left, Bignum *bn_right,
                      Bignum *bn_result);
void bn_subtract_eq(Bignum *bn_left, Bignum *bn_right);
Bignum *bn_subtract(Bignum *bn_left, Bignum *bn_right);

// left -= right
void bn_subtract_eq(Bignum *bn_left, Bignum *bn_right) {
    // simulate subtract_eq, sadly...
    Bignum *bn_result = bn_subtract(bn_left, bn_right);
    bn_copy(bn_left, bn_result);
    bn_free(bn_result);
}

// left += right
void bn_add_eq(Bignum *bn_left, Bignum *bn_right) {
    // simulate add_eq, sadly...
    Bignum *bn_result = bn_add(bn_left, bn_right);
    bn_copy(bn_left, bn_result);
    bn_free(bn_result);
}

void bn_subtract_no_malloc(Bignum *bn_left, Bignum *bn_right,
                           Bignum *bn_result) {
    size_t ll = bn_left->_length;
    size_t rl = bn_right->_length;
    bn_ensure_at_least(bn_result, ll > rl ? ll + 1 : rl + 1);
    bn_set(bn_result, 0);

    if (bn_left->_negative || bn_right->_negative) {
        bn_negate(bn_right);
        bn_add_no_malloc(bn_left, bn_right, bn_result);
        bn_negate(bn_right);
        return;
    }

    if (bn_compare(bn_left, bn_right) < 0) {
        bn_subtract_no_malloc(bn_right, bn_left, bn_result);
        bn_result->_negative = true;
        return;
    }

    // actually do subtraction
    char borrow = 0;
    bn_result->_length = ll > rl ? ll : rl;
    for (size_t idx = 0; idx <= bn_result->_length; idx++) {
        char v_left = bn_char_at(bn_left, idx);
        char v_right = bn_char_at(bn_right, idx);
        char sets = v_left - borrow - v_right;
        borrow = 0;
        if (sets < 0) {
            sets += 10;
            borrow = 1;
        }

        bn_result->_digits[idx] = sets;
    }
}

void bn_add_no_malloc(Bignum *bn_left, Bignum *bn_right,
                      Bignum *bn_result) {
    size_t ll = bn_left->_length;
    size_t rl = bn_right->_length;
    bn_ensure_at_least(bn_result, ll > rl ? ll + 1 : rl + 1);
    bn_set(bn_result, 0);

    if (bn_left->_negative == bn_right->_negative) {
        bn_result->_negative = bn_left->_negative;
    } else {
        if (bn_left->_negative == true) {
            bn_negate(bn_left);
            bn_subtract_no_malloc(bn_right, bn_left, bn_result);
            bn_negate(bn_left);
        } else {
            bn_negate(bn_right);
            bn_subtract_no_malloc(bn_right, bn_left, bn_result);
            bn_negate(bn_right);
        }
        return;
    }

    char carry = 0;
    char sets = 0;
    bn_result->_length = ll > rl ? ll : rl;
    for (size_t idx = 0; idx < bn_result->_length; idx++) {
        char v_left = bn_char_at(bn_left, idx);
        char v_right = bn_char_at(bn_right, idx);

        sets = (v_left + v_right + carry) % 10;
        bn_result->_digits[idx] = sets;
        carry = (v_left + v_right - sets) / 10;
    }
    if (carry) {
        bn_result->_digits[bn_result->_length] = 1;
        bn_result->_length += 1;
    }
}

Bignum *bn_add(Bignum *bn_left, Bignum *bn_right) {
    size_t ll = bn_left->_length;
    size_t rl = bn_right->_length;
    Bignum *bn_result = bn_make(ll > rl ? ll + 1 : rl + 1);

    bn_add_no_malloc(bn_left, bn_right, bn_result);
    return bn_result;
}


// left - right
Bignum *bn_subtract(Bignum *bn_left, Bignum *bn_right) {
    size_t ll = bn_left->_length;
    size_t rl = bn_right->_length;
    Bignum *bn_result = bn_make(ll > rl ? ll : rl);
    bn_subtract_no_malloc(bn_left, bn_right, bn_result);
    return bn_result;
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
