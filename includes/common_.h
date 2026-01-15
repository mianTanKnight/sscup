//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_COMMON__H
#define SCCPU_COMMON__H
#define BYTE_SIZE 8
#define WORD_SIZE 32
#include "stdint.h"
typedef _Bool bit;
typedef bit byte[BYTE_SIZE];
typedef bit word[WORD_SIZE];

// word[0]=MSB

const static byte BYTE_ONE = {0, 0, 0, 0, 0, 0, 0, 1};
const static byte BYTE_ZERO = {0};
const static word WORD_ONE = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};
const static word NOP = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
const static word WORD_ZERO = {0};


static inline
uint8_t u8_from_byte(const byte b) {
    uint8_t v = 0;
    for (int i = 0; i < BYTE_SIZE; i++) {
        if (b[i]) {
            v |= (uint8_t) (1u << (BYTE_SIZE - 1 - i));
        }
    }
    return v;
}

static inline
void connect(const word src, word dest) {
    for (int i = 0; i < WORD_SIZE; i++) {
        dest[i] = src[i];
    }
}


static inline
uint32_t u32_from_word(const word b) {
    uint32_t v = 0;
    for (int i = 0; i < WORD_SIZE; i++) {
        if (b[i]) {
            v |= (uint32_t) (1u << (WORD_SIZE - 1 - i));
        }
    }
    return v;
}

static inline
bit word_is_zero(const word b) {
    bit mask = 0;
    for (int i = 0; i < BYTE_SIZE; i++) {
        mask |= b[i];
    }
    return !mask;
}


// 逻辑 bit n (31..0) -> 存储下标 index = 31 - n
#define INST_BIT(instr, n)  ((instr)[WORD_SIZE - 1 - (n)])
#define INST_WORD(n) (WORD_SIZE - 1 - (n))

// word[0]=MSB
static inline void word_lshift2(const word in, word out) {
    // out = in << 2（按你 word[0]=MSB 的约定）
    // 逻辑 bit n(31..0) -> 存储 index = 31-n
    for (int n = 31; n >= 2; --n) {
        out[INST_WORD(n)] = in[INST_WORD(n - 2)];
    }
    out[INST_WORD(1)] = 0;
    out[INST_WORD(0)] = 0;
}

#endif //SCCPU_COMMON__H
