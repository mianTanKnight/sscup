// test_alu.c
// ALU tests: ADD, SUB, SLT, AND, OR, XOR, NOR + golden model fuzz

#include <stdio.h>
#include <stdint.h>
#include "../includes/alu.h"
#include "common_test.h"

// ======================== Helpers ========================

static inline uint32_t alu_compute(uint32_t a, uint32_t b, const ops op) {
    word wa = {0}, wb = {0}, wr = {0};
    bit overflow = 0;
    word_from_u32(a, wa);
    word_from_u32(b, wb);
    word_alu_(wa, wb, wr, op, &overflow);
    return word_to_u32(wr);
}

// Golden model for ALU
static inline uint32_t golden_alu(uint32_t a, uint32_t b, const ops op) {
    if (op[0] == 0 && op[1] == 0 && op[2] == 0) return a & b;       // AND
    if (op[0] == 0 && op[1] == 0 && op[2] == 1) return a | b;       // OR
    if (op[0] == 0 && op[1] == 1 && op[2] == 0) return a ^ b;       // XOR
    if (op[0] == 0 && op[1] == 1 && op[2] == 1) return ~(a | b);    // NOR
    if (op[0] == 1 && op[1] == 0 && op[2] == 0) return a + b;       // ADD
    if (op[0] == 1 && op[1] == 0 && op[2] == 1) return a - b;       // SUB
    if (op[0] == 1 && op[1] == 1 && op[2] == 0)                     // SLT
        return ((int32_t)a < (int32_t)b) ? 1 : 0;
    return 0; // NULL
}

// ======================== Tests ========================

static int test_alu_add(void) {
    printf("\n=== test_alu_add ===\n");
    ASSERT_EQ_U32("5+7=12",       alu_compute(5, 7, OPS_ADD_), 12);
    ASSERT_EQ_U32("0+0=0",        alu_compute(0, 0, OPS_ADD_), 0);
    ASSERT_EQ_U32("0xFFFF+1",     alu_compute(0xFFFF, 1, OPS_ADD_), 0x10000);
    ASSERT_EQ_U32("-1+1=0",       alu_compute(0xFFFFFFFF, 1, OPS_ADD_), 0);
    ASSERT_EQ_U32("0x7FFFFFFF+1", alu_compute(0x7FFFFFFF, 1, OPS_ADD_), 0x80000000);
    return 0;
}

static int test_alu_sub(void) {
    printf("\n=== test_alu_sub ===\n");
    ASSERT_EQ_U32("7-5=2",   alu_compute(7, 5, OPS_SUB_), 2);
    ASSERT_EQ_U32("5-7=-2",  alu_compute(5, 7, OPS_SUB_), (uint32_t)-2);
    ASSERT_EQ_U32("0-0=0",   alu_compute(0, 0, OPS_SUB_), 0);
    ASSERT_EQ_U32("0-1=-1",  alu_compute(0, 1, OPS_SUB_), 0xFFFFFFFF);
    return 0;
}

static int test_alu_slt(void) {
    printf("\n=== test_alu_slt ===\n");
    // signed comparison
    ASSERT_EQ_U32("SLT(3,5)=1",    alu_compute(3, 5, OPS_SLT_), 1);
    ASSERT_EQ_U32("SLT(5,3)=0",    alu_compute(5, 3, OPS_SLT_), 0);
    ASSERT_EQ_U32("SLT(5,5)=0",    alu_compute(5, 5, OPS_SLT_), 0);
    ASSERT_EQ_U32("SLT(-1,0)=1",   alu_compute(0xFFFFFFFF, 0, OPS_SLT_), 1);
    ASSERT_EQ_U32("SLT(0,-1)=0",   alu_compute(0, 0xFFFFFFFF, OPS_SLT_), 0);
    ASSERT_EQ_U32("SLT(-2,-1)=1",  alu_compute(0xFFFFFFFE, 0xFFFFFFFF, OPS_SLT_), 1);
    return 0;
}

static int test_alu_logic(void) {
    printf("\n=== test_alu_logic ===\n");
    ASSERT_EQ_U32("AND(0xFF00,0x0FF0)", alu_compute(0xFF00, 0x0FF0, OPS_AND_), 0x0F00);
    ASSERT_EQ_U32("OR(0xFF00,0x0FF0)",  alu_compute(0xFF00, 0x0FF0, OPS_OR_),  0xFFF0);
    ASSERT_EQ_U32("XOR(0xFF00,0x0FF0)", alu_compute(0xFF00, 0x0FF0, OPS_XOR_), 0xF0F0);
    ASSERT_EQ_U32("NOR(0xFF00,0x0FF0)", alu_compute(0xFF00, 0x0FF0, OPS_NOR_), ~(uint32_t)0xFFF0);
    return 0;
}

static int test_alu_fuzz_golden(void) {
    printf("\n=== test_alu_fuzz_golden (5000 cases) ===\n");
    uint32_t seed = 0xDEADC0DEu;

    const ops *all_ops[8] = {
        &OPS_AND_, &OPS_OR_, &OPS_XOR_, &OPS_NOR_,
        &OPS_ADD_, &OPS_SUB_, &OPS_SLT_, &OPS_NULL_
    };

    for (int i = 0; i < 5000; i++) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t a = seed;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t b = seed;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        int op_idx = (int)(seed % 8);

        uint32_t got = alu_compute(a, b, *all_ops[op_idx]);
        uint32_t exp = golden_alu(a, b, *all_ops[op_idx]);

        if (got != exp) {
            printf("[FAIL] fuzz i=%d op=%d a=0x%08X b=0x%08X got=0x%08X exp=0x%08X\n",
                   i, op_idx, a, b, got, exp);
            return 1;
        }
    }

    PASS("fuzz 5000 matched golden model");
    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_alu_add);
    RUN_TEST(test_alu_sub);
    RUN_TEST(test_alu_slt);
    RUN_TEST(test_alu_logic);
    RUN_TEST(test_alu_fuzz_golden);
    TEST_SUMMARY("ALU Tests");
    return rc;
}
