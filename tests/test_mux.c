// test_mux.c
// Multiplexer tests: 1-bit mux, byte mux, word mux

#include <stdio.h>
#include "../includes/mux.h"
#include "common_test.h"

// ======================== Tests ========================

static int test_mux2_1_bit(void) {
    printf("\n=== test_mux2_1_bit ===\n");
    // sel=0 -> input0, sel=1 -> input1
    ASSERT_EQ_BIT("mux(0,1,0)==0", mux2_1(0, 1, 0), 0);
    ASSERT_EQ_BIT("mux(0,1,1)==1", mux2_1(0, 1, 1), 1);
    ASSERT_EQ_BIT("mux(1,0,0)==1", mux2_1(1, 0, 0), 1);
    ASSERT_EQ_BIT("mux(1,0,1)==0", mux2_1(1, 0, 1), 0);
    // same inputs
    ASSERT_EQ_BIT("mux(1,1,0)==1", mux2_1(1, 1, 0), 1);
    ASSERT_EQ_BIT("mux(0,0,1)==0", mux2_1(0, 0, 1), 0);
    return 0;
}

static int test_byte_mux(void) {
    printf("\n=== test_byte_mux ===\n");
    byte a = {1,0,1,0,1,0,1,0}; // 0xAA
    byte b = {0,1,0,1,0,1,0,1}; // 0x55
    byte out = {0};

    // sel=0 -> a
    byte_mux_2_1(a, b, 0, out);
    for (int i = 0; i < BYTE_SIZE; i++) {
        if (out[i] != a[i]) {
            printf("[FAIL] byte_mux sel=0 bit %d\n", i);
            return 1;
        }
    }
    PASS("byte_mux sel=0 => input0");

    // sel=1 -> b
    byte_mux_2_1(a, b, 1, out);
    for (int i = 0; i < BYTE_SIZE; i++) {
        if (out[i] != b[i]) {
            printf("[FAIL] byte_mux sel=1 bit %d\n", i);
            return 1;
        }
    }
    PASS("byte_mux sel=1 => input1");

    return 0;
}

static int test_word_mux(void) {
    printf("\n=== test_word_mux ===\n");
    word a = {0}, b = {0}, out = {0};
    word_from_u32(0xDEADBEEF, a);
    word_from_u32(0xCAFEBABE, b);

    // sel=0 -> a
    word_mux_2_1(a, b, 0, out);
    ASSERT_EQ_U32("word_mux sel=0 => 0xDEADBEEF", word_to_u32(out), 0xDEADBEEF);

    // sel=1 -> b
    word_mux_2_1(a, b, 1, out);
    ASSERT_EQ_U32("word_mux sel=1 => 0xCAFEBABE", word_to_u32(out), 0xCAFEBABE);

    return 0;
}

// Exhaustive: 1-bit mux covers all 8 combinations (i0, i1, sel)
static int test_mux_exhaustive(void) {
    printf("\n=== test_mux_exhaustive ===\n");
    for (int i0 = 0; i0 <= 1; i0++) {
        for (int i1 = 0; i1 <= 1; i1++) {
            for (int s = 0; s <= 1; s++) {
                bit expected = s ? i1 : i0;
                bit got = mux2_1(i0, i1, s);
                char name[64];
                snprintf(name, sizeof(name), "mux(%d,%d,%d)==%d", i0, i1, s, expected);
                ASSERT_EQ_BIT(name, got, expected);
            }
        }
    }
    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_mux2_1_bit);
    RUN_TEST(test_byte_mux);
    RUN_TEST(test_word_mux);
    RUN_TEST(test_mux_exhaustive);
    TEST_SUMMARY("Multiplexer Tests");
    return rc;
}
