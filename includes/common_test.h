//
// Created by wenshen on 2026/1/9.
//
#ifndef SCCPU_COMMON_TEST_H
#define SCCPU_COMMON_TEST_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common_.h"
#include "reg_.h"
#include "gate_.h"
#include "pc_.h"
#include "decoder_.h"
#include "if_id_.h"
#include "id_ex_.h"
#include "ex_mem.h"
#include "im_.h"

// 约定：word[0] 是 MSB，word[31] 是 LSB（与你 INST_BIT/INST_WORD 宏一致）
static inline void word_zero(word w) {
    for (int i = 0; i < WORD_SIZE; i++) w[i] = 0;
}

static inline uint32_t u32_from_word_test(const word w) {
    uint32_t v = 0;
    for (int i = 0; i < WORD_SIZE; i++) {
        if (w[i]) v |= (1u << (WORD_SIZE - 1 - i));
    }
    return v;
}

static inline void word_from_u32(uint32_t v, word w) {
    for (int i = 0; i < WORD_SIZE; ++i) {
        int bit_index = (WORD_SIZE - 1 - i); // i=0->bit31, i=31->bit0
        w[i] = (v >> bit_index) & 1u;
    }
}

static inline uint32_t u32_from_word_local(const word w) {
    uint32_t v = 0;
    for (int i = 0; i < WORD_SIZE; ++i) {
        if (w[i]) {
            v |= (1u << (WORD_SIZE - 1 - i));
        }
    }
    return v;
}

static inline void zero_word(word w) {
    for (int i = 0; i < WORD_SIZE; ++i) w[i] = 0;
}

static inline void reg32_write_u32(Reg32_ *r, uint32_t v) {
    word din = {0};
    word out = {0};
    word_from_u32(v, din);
    reg32_step(r, 1, din, out, 0);
    reg32_step(r, 1, din, out, 1);
}

static inline uint32_t reg32_read_u32(const Reg32_ *r) {
    word w = {0};
    read_reg32((Reg32_ *) r, w);
    return u32_from_word_local(w);
}

static inline void print_word_hex(const word w) {
    printf("0x%08X", (unsigned) u32_from_word_local(w));
}

static inline void read_pc_u32(Pc32_ *pc, uint32_t *out) {
    word w = {0};
    read_reg32(&pc->reg32, w);
    *out = u32_from_word_local(w);
}

static inline void read_ifid_instr_u32(If_id_regs *r, uint32_t *out) {
    word w = {0};
    read_reg32(&r->instr, w);
    *out = u32_from_word_local(w);
}

static inline void read_ifid_pc4_u32(If_id_regs *r, uint32_t *out) {
    word w = {0};
    read_reg32(&r->pc_plus4, w);
    *out = u32_from_word_local(w);
}

static inline void im_set_u32(Im_t *im, uint32_t index, uint32_t inst) {
    word_from_u32(inst, im->im[index]);
}


static inline bit BITN(const word w, int n) {
    // n = 31..0
    return INST_BIT(w, n) & 1;
}

static inline void reg32_write_now(Reg32_ *r, uint32_t v) {
    word din = {0};
    word out = {0};
    word_from_u32(v, din);
    // clk=0 准备；clk=1 提交
    reg32_step(r, 1, din, out, 0);
    reg32_step(r, 1, din, out, 1);
}

#define PASS(msg) do { printf("[PASS] %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("[FAIL] %s\n", (msg)); return 1; } while (0)

#define ASSERT_EQ_U32(name, actual, expected) do { \
if ((uint32_t)(actual) != (uint32_t)(expected)) { \
printf("[FAIL] %s: got=0x%08X (%u), expected=0x%08X (%u)\n", \
(name), (uint32_t)(actual), (uint32_t)(actual), (uint32_t)(expected), (uint32_t)(expected)); \
return 1; \
} else { \
printf("[PASS] %s\n", (name)); \
} \
} while (0)

#define ASSERT_EQ_BIT(name, actual, expected) do { \
if (((actual)&1) != ((expected)&1)) { \
printf("[FAIL] %s: got=%d, expected=%d\n", (name), (int)((actual)&1), (int)((expected)&1)); \
return 1; \
} else { \
printf("[PASS] %s\n", (name)); \
} \
} while (0)


#endif //SCCPU_COMMON_TEST_H
