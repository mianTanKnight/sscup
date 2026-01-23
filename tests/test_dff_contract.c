//
// Created by wenshen on 2026/1/23.
// test_dff_contract.c
#include <stdio.h>

#include "common_test.h"
#include "../includes/mux.h"
#include "../includes/dff.h"
#include "../includes/reg.h"
#include "../includes/utils.h"

#define PASS(msg) do { printf("[PASS] %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("[FAIL] %s\n", (msg)); return 1; } while (0)
#define ASSERT_EQ_BIT(name, actual, expected) do { \
int a__ = ((actual) & 1); \
int e__ = ((expected) & 1); \
if (a__ != e__) { \
printf("[FAIL] %s: got=%d, expected=%d\n", (name), a__, e__); \
return 1; \
} else { \
printf("[PASS] %s\n", (name)); \
} \
} while (0)

static int test_dff_commit_dontcare_din(void) {
    printf("=== test_dff_commit_dontcare_din ===\n");

    dff_b_ d;
    init_dff_deh(&d);

    // phase0: sample A=1 at clk=0
    (void)dff_deh_step(&d, 0, 1);

    // phase1: commit at clk=1 but we intentionally change din to B=0
    bit q = dff_deh_step(&d, 1, 0);

    // Contract expectation: q should be A (=1), meaning clk=1 ignores din
    // Your current implementation will produce q=0 -> FAIL
    ASSERT_EQ_BIT("Q must equal A (clk=1 din don't care)", q, 1);

    return 0;
}

static inline uint32_t reg32_read_u32_local(const Reg32_ *r) {
    word w = {0};
    read_reg32(r, w);
    return u32_from_word_local(w);
}



// 你 common_test.h 里已经有：word_from_u32 / u32_from_word_local / init_reg32 / read_reg32
// 以及 ASSERT_EQ_U32 / PASS / FAIL

static inline void reg32_two_phase_write(Reg32_ *r, uint32_t a, uint32_t b) {
    word dinA = {0};
    word dinB = {0};
    word out  = {0};

    word_from_u32(a, dinA);
    word_from_u32(b, dinB);

    // phase0: sample A into master
    reg32_step(r, 1, dinA, out, 0);

    // phase1: commit, but we intentionally provide B as din
    // Contract requires: din MUST NOT affect commit result.
    reg32_step(r, 1, dinB, out, 1);
}

static int test_reg32_commit_dontcare_din(void) {
    printf("=== test_reg32_commit_dontcare_din ===\n");

    Reg32_ r;
    init_reg32(&r);

    // Start clean: Q should be 0
    ASSERT_EQ_U32("init Q==0", reg32_read_u32_local(&r), 0);

    // Case 1
    {
        uint32_t A = 0xA5A5A5A5u;
        uint32_t B = 0x5A5A5A5Au;
        reg32_two_phase_write(&r, A, B);
        ASSERT_EQ_U32("Q must equal A (din@clk=1 ignored)", reg32_read_u32_local(&r), A);
    }

    // Case 2: random-ish patterns
    {
        uint32_t A = 0xDEADBEEFu;
        uint32_t B = 0x12345678u;
        reg32_two_phase_write(&r, A, B);
        ASSERT_EQ_U32("Q must equal A again", reg32_read_u32_local(&r), A);
    }

    // Case 3: we=0 should prevent both sample and commit
    {
        word dinA = {0};
        word dinB = {0};
        word out  = {0};
        uint32_t prev = reg32_read_u32_local(&r);

        word_from_u32(0xFFFFFFFFu, dinA);
        word_from_u32(0x00000000u, dinB);

        reg32_step(&r, 0, dinA, out, 0); // no sample
        reg32_step(&r, 0, dinB, out, 1); // no commit
        ASSERT_EQ_U32("we=0 => Q unchanged", reg32_read_u32_local(&r), prev);
    }

    // Case 4: only calling clk=1 without prior clk=0 should not "sample"
    // (depends on your DFF design; usually commit uses prior master state)
    {
        Reg32_ t;
        init_reg32(&t);

        word dinB = {0};
        word out  = {0};
        word_from_u32(0xCAFEBABEu, dinB);

        // commit-only without sample; expected still 0 if master starts 0
        reg32_step(&t, 1, dinB, out, 1);
        ASSERT_EQ_U32("clk=1 alone must not sample din", reg32_read_u32_local(&t), 0);
    }

    printf("[PASS] reg32 two-phase contract holds ✅\n");
    return 0;
}


// int main(void) {
//     int rc = 0;
//     rc |= test_dff_commit_dontcare_din();
//     rc |= test_reg32_commit_dontcare_din();
//     if (rc == 0) printf("ALL DFF CONTRACT TESTS PASSED ✅\n");
//     return rc;
// }
