// test_dff_contract.c
// DFF two-phase clock contract verification
// Tests: dff_deh master-slave semantics, reg32 two-phase write, we=0 guard

#include <stdio.h>
#include "../includes/dff.h"
#include "common_test.h"

// -------------------------
// Test 1: clk=1 must use value sampled at clk=0, ignoring current din
// -------------------------
static int test_dff_commit_dontcare_din(void) {
    printf("=== test_dff_commit_dontcare_din ===\n");

    dff_b_ d;
    init_dff_deh(&d);

    // phase0: sample A=1 at clk=0
    (void)dff_deh_step(&d, 0, 1);

    // phase1: commit at clk=1 but intentionally change din to B=0
    bit q = dff_deh_step(&d, 1, 0);

    // Contract: q should be A (=1), clk=1 ignores din
    ASSERT_EQ_BIT("Q must equal A (clk=1 din don't care)", q, 1);

    return 0;
}

// -------------------------
// Helper: write different values at phase0 vs phase1
// -------------------------
static inline void reg32_two_phase_write(Reg32_ *r, uint32_t a, uint32_t b) {
    word dinA = {0}, dinB = {0}, out = {0};
    word_from_u32(a, dinA);
    word_from_u32(b, dinB);

    // phase0: sample A into master
    reg32_step(r, 1, dinA, out, 0);
    // phase1: commit, but provide B as din (must be ignored)
    reg32_step(r, 1, dinB, out, 1);
}

// -------------------------
// Test 2: reg32 two-phase contract (din at clk=1 must not affect result)
// -------------------------
static int test_reg32_commit_dontcare_din(void) {
    printf("=== test_reg32_commit_dontcare_din ===\n");

    Reg32_ r;
    init_reg32(&r);

    // Start clean: Q should be 0
    ASSERT_EQ_U32("init Q==0", reg32_read_u32(&r), 0);

    // Case 1: write A, provide B at commit
    {
        uint32_t A = 0xA5A5A5A5u;
        uint32_t B = 0x5A5A5A5Au;
        reg32_two_phase_write(&r, A, B);
        ASSERT_EQ_U32("Q must equal A (din@clk=1 ignored)", reg32_read_u32(&r), A);
    }

    // Case 2: different patterns
    {
        uint32_t A = 0xDEADBEEFu;
        uint32_t B = 0x12345678u;
        reg32_two_phase_write(&r, A, B);
        ASSERT_EQ_U32("Q must equal A again", reg32_read_u32(&r), A);
    }

    // Case 3: we=0 should prevent both sample and commit
    {
        word dinA = {0}, dinB = {0}, out = {0};
        uint32_t prev = reg32_read_u32(&r);

        word_from_u32(0xFFFFFFFFu, dinA);
        word_from_u32(0x00000000u, dinB);

        reg32_step(&r, 0, dinA, out, 0); // no sample
        reg32_step(&r, 0, dinB, out, 1); // no commit
        ASSERT_EQ_U32("we=0 => Q unchanged", reg32_read_u32(&r), prev);
    }

    // Case 4: only clk=1 without prior clk=0 should not "sample"
    {
        Reg32_ t;
        init_reg32(&t);

        word dinB = {0}, out = {0};
        word_from_u32(0xCAFEBABEu, dinB);

        // commit-only without sample; expected still 0
        reg32_step(&t, 1, dinB, out, 1);
        ASSERT_EQ_U32("clk=1 alone must not sample din", reg32_read_u32(&t), 0);
    }

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_dff_commit_dontcare_din);
    RUN_TEST(test_reg32_commit_dontcare_din);
    TEST_SUMMARY("DFF Contract Tests");
    return rc;
}
