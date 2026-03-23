// test_beq_forwarding.c
// Integration tests: BEQ + forwarding, load-use, flush next store

#include <stdio.h>
#include "../includes/cpu_core.h"
#include "common_test.h"

static void load_u32_inst(word dst, uint32_t inst) {
    u32_to_word(inst, dst);
}

// ======================== Tests ========================

static int test_beq_load_use_must_stall(void) {
    printf("\n=== test_beq_load_use_must_stall ===\n");
    Cpu_core c;
    init_cpu_c(&c);

    bit mask_all[4] = {1, 1, 1, 1};
    dm_write_u32_local(&c.dm, 0, 5, mask_all, 1, 1);

    word program[6] = {0};
    // 0: lw   r1, 0(r0)   -> r1 = 5
    load_u32_inst(program[0], enc_i(OP_LW, 0, 1, 0));
    load_u32_inst(program[1], 0);
    // 2: beq  r1, r0, +1  -> NOT taken (r1=5 != r0=0)
    load_u32_inst(program[2], enc_beq(1, 0, 1));
    // 3: addi r2, r0, 9   -> MUST execute, r2=9
    load_u32_inst(program[3], enc_addi(2, 0, 9));
    // 4: addi r3, r0, 7
    load_u32_inst(program[4], enc_addi(3, 0, 7));
    load_u32_inst(program[5], 0);

    im_load_program(&c.im, program, 6);

    for (int i = 0; i < 20; ++i)
        cpu_tick(&c);

    ASSERT_EQ_U32("R1 should be 5 after LW", reg32_read_u32(&c.rf.r1), 5);
    ASSERT_EQ_U32("R2 should be 9 (BEQ must NOT be taken)", reg32_read_u32(&c.rf.r2), 9);

    return 0;
}

static int test_beq_taken_flushes_next_store(void) {
    printf("\n=== test_beq_taken_flushes_next_store ===\n");
    Cpu_core c;
    init_cpu_c(&c);

    word program[6] = {0};
    // 0: addi r1, r0, 1
    // 1: addi r2, r0, 1
    // 2: beq  r1, r2, +1   ; taken -> skip next
    // 3: sw   r1, 0(r0)    ; MUST be squashed
    // 4: addi r3, r0, 7    ; MUST execute
    load_u32_inst(program[0], enc_addi(1, 0, 1));
    load_u32_inst(program[1], enc_addi(2, 0, 1));
    load_u32_inst(program[2], enc_beq(1, 2, 1));
    load_u32_inst(program[3], enc_i(OP_SW, 0, 1, 0));
    load_u32_inst(program[4], enc_addi(3, 0, 7));
    load_u32_inst(program[5], 0);

    im_load_program(&c.im, program, 6);

    for (int i = 0; i < 20; ++i)
        cpu_tick(&c);

    ASSERT_EQ_U32("R1 == 1", reg32_read_u32(&c.rf.r1), 1);
    ASSERT_EQ_U32("R2 == 1", reg32_read_u32(&c.rf.r2), 1);
    ASSERT_EQ_U32("R3 == 7", reg32_read_u32(&c.rf.r3), 7);
    ASSERT_EQ_U8("DM[0] == 0 (SW flushed)", c.dm.memory[0], 0);

    return 0;
}

static int test_beq_taken_flushes_basic(void) {
    printf("\n=== test_beq_taken_flushes_basic ===\n");
    Cpu_core c;
    init_cpu_c(&c);

    word program[5] = {0};
    // 0: ADDI R1, R0, 1
    // 1: ADDI R2, R0, 1
    // 2: BEQ  R1, R2, +1   ; taken -> jump to 4
    // 3: SW   R1, 0(R0)    ; must be flushed
    // 4: NOP
    load_u32_inst(program[0], enc_addi(1, 0, 1));
    load_u32_inst(program[1], enc_addi(2, 0, 1));
    load_u32_inst(program[2], enc_beq(1, 2, 1));
    load_u32_inst(program[3], enc_i(OP_SW, 0, 1, 0));
    load_u32_inst(program[4], 0);

    im_load_program(&c.im, program, 5);

    for (int i = 0; i < 12; ++i)
        cpu_tick(&c);

    // DM[0] must still be 0 (SW was flushed)
    word addr0 = {0}, out = {0};
    u32_to_word(0u, addr0);
    bit err = c.dm.m_read(&c.dm, addr0, out);
    ASSERT_EQ_BIT("read err==0", err, 0);
    ASSERT_EQ_U32("DM[0]==0 (SW flushed)", word_to_u32(out), 0u);

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_beq_load_use_must_stall);
    RUN_TEST(test_beq_taken_flushes_next_store);
    RUN_TEST(test_beq_taken_flushes_basic);
    TEST_SUMMARY("BEQ + Forwarding Integration Tests");
    return rc;
}
