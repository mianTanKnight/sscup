// test_pc.c
// Program Counter tests: PC+4 increment, branch taken, reset

#include <stdio.h>
#include "../includes/pc.h"
#include "common_test.h"

// ======================== Helpers ========================

// Two-phase tick for PC
static inline void pc_tick(Pc32_ *pc, bit reset, const word init,
                           bit branch_taken, const word branch_target,
                           word pc_out, bit *overflow) {
    pc32_word_step(pc, reset, init, branch_taken, branch_target, pc_out, overflow, 0);
    pc32_word_step(pc, reset, init, branch_taken, branch_target, pc_out, overflow, 1);
}

// ======================== Tests ========================

static int test_pc_init_zero(void) {
    printf("\n=== test_pc_init_zero ===\n");
    Pc32_ pc;
    init_pc32(&pc);
    ASSERT_EQ_U32("PC init == 0", reg32_read_u32(&pc.reg32), 0);
    return 0;
}

static int test_pc_plus4_sequential(void) {
    printf("\n=== test_pc_plus4_sequential ===\n");
    Pc32_ pc;
    init_pc32(&pc);

    word pc_out = {0};
    word branch_target = {0};
    word init_val = {0};
    bit ov = 0;

    // 5 sequential increments: 0->4->8->12->16->20
    for (int i = 0; i < 5; i++) {
        pc_tick(&pc, 0, init_val, 0, branch_target, pc_out, &ov);

        char name[64];
        snprintf(name, sizeof(name), "PC after tick %d == %d", i+1, (i+1)*4);
        ASSERT_EQ_U32(name, reg32_read_u32(&pc.reg32), (uint32_t)((i+1)*4));
    }

    return 0;
}

static int test_pc_branch_taken(void) {
    printf("\n=== test_pc_branch_taken ===\n");
    Pc32_ pc;
    init_pc32(&pc);

    word pc_out = {0}, init_val = {0};
    bit ov = 0;

    // First tick: PC 0->4
    word no_branch = {0};
    pc_tick(&pc, 0, init_val, 0, no_branch, pc_out, &ov);
    ASSERT_EQ_U32("PC==4 after normal tick", reg32_read_u32(&pc.reg32), 4);

    // Branch to 0x100
    word target = {0};
    word_from_u32(0x100, target);
    pc_tick(&pc, 0, init_val, 1, target, pc_out, &ov);
    ASSERT_EQ_U32("PC==0x100 after branch", reg32_read_u32(&pc.reg32), 0x100);

    // Continue normal: 0x100 -> 0x104
    pc_tick(&pc, 0, init_val, 0, no_branch, pc_out, &ov);
    ASSERT_EQ_U32("PC==0x104 after branch+4", reg32_read_u32(&pc.reg32), 0x104);

    return 0;
}

static int test_pc_reset(void) {
    printf("\n=== test_pc_reset ===\n");
    Pc32_ pc;
    init_pc32(&pc);

    word pc_out = {0}, init_val = {0}, no_branch = {0};
    bit ov = 0;

    // Advance to 8
    pc_tick(&pc, 0, init_val, 0, no_branch, pc_out, &ov);
    pc_tick(&pc, 0, init_val, 0, no_branch, pc_out, &ov);
    ASSERT_EQ_U32("PC==8 before reset", reg32_read_u32(&pc.reg32), 8);

    // Reset to 0
    pc_tick(&pc, 1, init_val, 0, no_branch, pc_out, &ov);
    ASSERT_EQ_U32("PC==0 after reset", reg32_read_u32(&pc.reg32), 0);

    // Reset to custom value
    word custom = {0};
    word_from_u32(0x200, custom);
    pc_tick(&pc, 1, custom, 0, no_branch, pc_out, &ov);
    ASSERT_EQ_U32("PC==0x200 after custom reset", reg32_read_u32(&pc.reg32), 0x200);

    return 0;
}

// Reset has higher priority than branch
static int test_pc_reset_over_branch(void) {
    printf("\n=== test_pc_reset_over_branch ===\n");
    Pc32_ pc;
    init_pc32(&pc);

    word pc_out = {0};
    bit ov = 0;

    word init_val = {0}, target = {0};
    word_from_u32(0x0, init_val);
    word_from_u32(0x300, target);

    // Both reset=1 and branch_taken=1: reset should win
    pc_tick(&pc, 1, init_val, 1, target, pc_out, &ov);
    ASSERT_EQ_U32("reset wins over branch", reg32_read_u32(&pc.reg32), 0x0);

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_pc_init_zero);
    RUN_TEST(test_pc_plus4_sequential);
    RUN_TEST(test_pc_branch_taken);
    RUN_TEST(test_pc_reset);
    RUN_TEST(test_pc_reset_over_branch);
    TEST_SUMMARY("Program Counter Tests");
    return rc;
}
