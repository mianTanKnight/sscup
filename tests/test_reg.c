// test_reg.c
// Register and register file tests: reg32 read/write, regfile addressing, isolation

#include <stdio.h>
#include "../includes/reg.h"
#include "common_test.h"

// ======================== Tests ========================

static int test_reg32_init_zero(void) {
    printf("\n=== test_reg32_init_zero ===\n");
    Reg32_ r;
    init_reg32(&r);
    ASSERT_EQ_U32("init == 0", reg32_read_u32(&r), 0);
    return 0;
}

static int test_reg32_write_read(void) {
    printf("\n=== test_reg32_write_read ===\n");
    Reg32_ r;
    init_reg32(&r);

    reg32_write_u32(&r, 0xDEADBEEF);
    ASSERT_EQ_U32("read back DEADBEEF", reg32_read_u32(&r), 0xDEADBEEF);

    reg32_write_u32(&r, 0x12345678);
    ASSERT_EQ_U32("read back 12345678", reg32_read_u32(&r), 0x12345678);

    reg32_write_u32(&r, 0);
    ASSERT_EQ_U32("read back 0", reg32_read_u32(&r), 0);

    return 0;
}

static int test_reg32_we_guard(void) {
    printf("\n=== test_reg32_we_guard ===\n");
    Reg32_ r;
    init_reg32(&r);

    reg32_write_u32(&r, 0xAAAAAAAA);
    ASSERT_EQ_U32("initial write", reg32_read_u32(&r), 0xAAAAAAAA);

    // Try to write with we=0
    word din = {0}, out = {0};
    word_from_u32(0x55555555, din);
    reg32_step(&r, 0, din, out, 0);
    reg32_step(&r, 0, din, out, 1);
    ASSERT_EQ_U32("we=0 => unchanged", reg32_read_u32(&r), 0xAAAAAAAA);

    return 0;
}

static int test_regfile_init(void) {
    printf("\n=== test_regfile_init ===\n");
    Reg324file_ rf;
    init_reg32file(&rf);
    ASSERT_EQ_U32("R0 init == 0", reg32_read_u32(&rf.r0), 0);
    ASSERT_EQ_U32("R1 init == 0", reg32_read_u32(&rf.r1), 0);
    ASSERT_EQ_U32("R2 init == 0", reg32_read_u32(&rf.r2), 0);
    ASSERT_EQ_U32("R3 init == 0", reg32_read_u32(&rf.r3), 0);
    return 0;
}

static int test_regfile_write_read(void) {
    printf("\n=== test_regfile_write_read ===\n");
    Reg324file_ rf;
    init_reg32file(&rf);

    // Write to each register independently
    uint32_t vals[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

    for (int idx = 0; idx < 4; idx++) {
        bit a3[2] = {(bit)(idx & 1), (bit)((idx >> 1) & 1)};
        // a3[0] is low bit, a3[1] is high bit
        // idx=0: 00, idx=1: 01(a3[0]=1), idx=2: 10(a3[1]=1, a3[0]=0 -> but wait
        // The regfile uses a3_[0] as the high bit and a3_[1] as the low bit based on the decode:
        // reg0_e = AND(AND(NOT(a3_[0]), NOT(a3_[1])), we3)  -> a3={0,0} -> r0
        // reg1_e = AND(AND(NOT(a3_[0]), a3_[1]), we3)       -> a3={0,1} -> r1
        // reg2_e = AND(AND(a3_[0], NOT(a3_[1])), we3)       -> a3={1,0} -> r2
        // reg3_e = AND(AND(a3_[0], a3_[1]), we3)             -> a3={1,1} -> r3
        // So a3_[0] is high bit, a3_[1] is low bit
        a3[0] = (bit)((idx >> 1) & 1); // high bit
        a3[1] = (bit)(idx & 1);        // low bit

        word wd = {0}, rd1 = {0}, rd2 = {0};
        word_from_u32(vals[idx], wd);

        Regfile_in in = {.we3 = 1, .a1 = a3, .a2 = a3, .a3 = a3};
        memcpy(in.wd3, wd, sizeof(word));

        reg324file_step(&rf, &in, rd1, rd2, 0);
        reg324file_step(&rf, &in, rd1, rd2, 1);
    }

    ASSERT_EQ_U32("R0 == 0x11111111", reg32_read_u32(&rf.r0), 0x11111111);
    ASSERT_EQ_U32("R1 == 0x22222222", reg32_read_u32(&rf.r1), 0x22222222);
    ASSERT_EQ_U32("R2 == 0x33333333", reg32_read_u32(&rf.r2), 0x33333333);
    ASSERT_EQ_U32("R3 == 0x44444444", reg32_read_u32(&rf.r3), 0x44444444);

    return 0;
}

static int test_regfile_read_select(void) {
    printf("\n=== test_regfile_read_select ===\n");
    Reg324file_ rf;
    init_reg32file(&rf);

    // Pre-load registers
    reg32_write_u32(&rf.r0, 0xAA);
    reg32_write_u32(&rf.r1, 0xBB);
    reg32_write_u32(&rf.r2, 0xCC);
    reg32_write_u32(&rf.r3, 0xDD);

    uint32_t expected[4] = {0xAA, 0xBB, 0xCC, 0xDD};

    // Test all read address combinations for A1 and A2
    for (int a1_idx = 0; a1_idx < 4; a1_idx++) {
        for (int a2_idx = 0; a2_idx < 4; a2_idx++) {
            bit a1[2] = {(bit)((a1_idx >> 1) & 1), (bit)(a1_idx & 1)};
            bit a2[2] = {(bit)((a2_idx >> 1) & 1), (bit)(a2_idx & 1)};
            bit a3[2] = {0, 0};
            word wd = {0}, rd1 = {0}, rd2 = {0};

            Regfile_in in = {.we3 = 0, .a1 = a1, .a2 = a2, .a3 = a3};
            memcpy(in.wd3, wd, sizeof(word));

            reg324file_step(&rf, &in, rd1, rd2, 0);

            char name[64];
            snprintf(name, sizeof(name), "rd1[a1=%d]", a1_idx);
            ASSERT_EQ_U32(name, word_to_u32(rd1), expected[a1_idx]);

            snprintf(name, sizeof(name), "rd2[a2=%d]", a2_idx);
            ASSERT_EQ_U32(name, word_to_u32(rd2), expected[a2_idx]);
        }
    }

    return 0;
}

static int test_regfile_write_isolation(void) {
    printf("\n=== test_regfile_write_isolation ===\n");
    Reg324file_ rf;
    init_reg32file(&rf);

    reg32_write_u32(&rf.r0, 0x10);
    reg32_write_u32(&rf.r1, 0x20);
    reg32_write_u32(&rf.r2, 0x30);
    reg32_write_u32(&rf.r3, 0x40);

    // Write only to R2 via regfile_step
    bit a3[2] = {1, 0}; // a3_[0]=1, a3_[1]=0 -> R2
    word wd = {0}, rd1 = {0}, rd2 = {0};
    word_from_u32(0xFF, wd);

    Regfile_in in = {.we3 = 1, .a1 = a3, .a2 = a3, .a3 = a3};
    memcpy(in.wd3, wd, sizeof(word));

    reg324file_step(&rf, &in, rd1, rd2, 0);
    reg324file_step(&rf, &in, rd1, rd2, 1);

    ASSERT_EQ_U32("R0 unchanged", reg32_read_u32(&rf.r0), 0x10);
    ASSERT_EQ_U32("R1 unchanged", reg32_read_u32(&rf.r1), 0x20);
    ASSERT_EQ_U32("R2 updated",   reg32_read_u32(&rf.r2), 0xFF);
    ASSERT_EQ_U32("R3 unchanged", reg32_read_u32(&rf.r3), 0x40);

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_reg32_init_zero);
    RUN_TEST(test_reg32_write_read);
    RUN_TEST(test_reg32_we_guard);
    RUN_TEST(test_regfile_init);
    RUN_TEST(test_regfile_write_read);
    RUN_TEST(test_regfile_read_select);
    RUN_TEST(test_regfile_write_isolation);
    TEST_SUMMARY("Register & Register File Tests");
    return rc;
}
