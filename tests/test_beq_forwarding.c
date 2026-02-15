//
// Created by wenshen on 2026/2/2.
//

#include <assert.h>
#include <stdio.h>

#include "common_test.h"
#include "../includes/cpu_core.h"
#include "../includes/utils.h"

static void load_u32_inst(word dst, uint32_t inst) {
    u32_to_word(inst, dst);
}

static inline void assert_u32_eq(uint32_t a, uint32_t b, const char *msg) {
    if (a != b) {
        printf("ASSERT FAIL: %s (0x%08X != 0x%08X)\n", msg, a, b);
    }
}

void test_beq_load_use_must_stall(void) {
    Cpu_core c;
    init_cpu_c(&c);

    //  用“写 word”接口，避免端序污染
    bit mask_all[4] = {1, 1, 1, 1};
    dm_write_u32_local(&c.dm, 0, 5, mask_all, 1, 1);
    printf("test_beq_load_use_must_stall\n");

    word program[6] = {0};

    // 0: lw   r1, 0(r0)   -> r1 = 5
    load_u32_inst(program[0], enc_i(OP_LW, 0, 1, 0));
    load_u32_inst(program[1], 0);
    // 1: beq  r1, r0, +1  -> NOT taken (because r1=5)
    load_u32_inst(program[2], enc_beq(1, 0, 1));
    // 2: addi r2, r0, 9   -> MUST execute, r2=9
    load_u32_inst(program[3], enc_addi(2, 0, 9));
    // 3: addi r3, r0, 7
    load_u32_inst(program[4], enc_addi(3, 0, 7));
    load_u32_inst(program[5], 0);
    im_load_program(&c.im, program, 6);
    for (int i = 0; i < 20; ++i) {
        cpu_tick(&c);
    }
    assert_u32_eq(reg32_read_u32(&c.rf.r1), 5, "R1 should be 5 after LW");
    assert_u32_eq(reg32_read_u32(&c.rf.r2), 9, "R2 should be 9 (BEQ must NOT be taken)");
    printf("test_beq_load_use_must_stall passed!\n");
}


//
// // 目标：BEQ taken 后，下一条 SW 必须被 squash，不能写内存
void test_beq_taken_flushes_next_store(void) {
    Cpu_core c;
    init_cpu_c(&c);
    word program[6] = {0};
    //     // program:
    //     // 0: addi r1, r0, 1
    //     // 1: addi r2, r0, 1
    //     // 2: beq  r1, r2, +1      ; taken -> skip next instr
    //     // 3: sw   r1, 0(r0)       ; MUST be squashed
    //     // 4: addi r3, r0, 7       ; MUST execute
    load_u32_inst(program[0], enc_addi(1, 0, 1));
    load_u32_inst(program[1], enc_addi(2, 0, 1));
    load_u32_inst(program[2], enc_beq(1, 2, 1));
    load_u32_inst(program[3], enc_i(OP_SW, 0, 1, 0));
    load_u32_inst(program[4], enc_addi(3, 0, 7));
    load_u32_inst(program[5], 0); // NOP


    im_load_program(&c.im, program, 6);

    // Run enough cycles so SW (if not flushed) would reach MEM stage and write DM.
    for (int i = 0; i < 20; ++i) {
        cpu_tick(&c);
    }
    assert(reg32_read_u32(&c.rf.r1) == 1);
    assert(reg32_read_u32(&c.rf.r2) == 1);
    assert(reg32_read_u32(&c.rf.r3) == 7);
    assert(c.dm.memory[0] == 0);
    printf("test_beq_taken_flushes_next_store passed!\n");
}

static
inline void test_ebq_taken_flushes(void) {
    Cpu_core c;
    init_cpu_c(&c);

    // Program:
    // 0:  ADDI R1, R0, 1
    // 4:  ADDI R2, R0, 1
    // 8:  BEQ  R1, R2, +1     ; should jump to 16, skipping SW
    // 12: SW   R1, 0(R0)      ; must be flushed if branch taken
    // 16: NOP
    word program[5] = {0};

    load_u32_inst(program[0], enc_addi(1, 0, 1));
    load_u32_inst(program[1], enc_addi(2, 0, 1));
    load_u32_inst(program[2], enc_beq(1, 2, 1));
    load_u32_inst(program[3], enc_i(OP_SW, 0, 1, 0));
    load_u32_inst(program[4], 0); // NOP

    im_load_program(&c.im, program, 5);

    // Run enough cycles so SW (if not flushed) would reach MEM stage and write DM.
    for (int i = 0; i < 12; ++i) {
        cpu_tick(&c);
    }

    // Read DM[0]
    word addr0 = {0};
    u32_to_word(0u, addr0);

    word out = {0};
    bit err = c.dm.m_read(&c.dm, addr0, out);
    assert(err == 0);

    uint32_t v = u32_from_word(out);
    // Correct behavior: branch taken => SW flushed => memory stays 0
    // Bug behavior: branch not taken => SW executed => memory becomes 1
    assert(v == 0u);

    printf("test_beq_forwarding passed!\n");
}


//
int main() {
    test_beq_load_use_must_stall();
}
