//
// Created by wenshen on 2026/2/2.
//

#include <assert.h>
#include <stdio.h>
#include "../includes/cpu_core.h"
#include "../includes/utils.h"

static void load_u32_inst(word dst, uint32_t inst) {
    u32_to_word(inst, dst);
}
//
// int main11() {
//     Cpu_core c;
//     init_cpu_c(&c);
//
//     // Program:
//     // 0:  ADDI R1, R0, 1
//     // 4:  ADDI R2, R0, 1
//     // 8:  BEQ  R1, R2, +1     ; should jump to 16, skipping SW
//     // 12: SW   R1, 0(R0)      ; must be flushed if branch taken
//     // 16: NOP
//     word program[5] = {0};
//
//     load_u32_inst(program[0], enc_addi(1, 0, 1));
//     load_u32_inst(program[1], enc_addi(2, 0, 1));
//     load_u32_inst(program[2], enc_beq(1, 2, 1));
//     load_u32_inst(program[3], enc_i(OP_SW, 0, 1, 0));
//     load_u32_inst(program[4], 0); // NOP
//
//     im_load_program(&c.im, program, 5);
//
//     // Run enough cycles so SW (if not flushed) would reach MEM stage and write DM.
//     for (int i = 0; i < 12; ++i) {
//         cpu_tick(&c);
//     }
//
//     // Read DM[0]
//     word addr0 = {0};
//     u32_to_word(0u, addr0);
//
//     word out = {0};
//     bit err = c.dm.m_read(&c.dm, addr0, out);
//     assert(err == 0);
//
//     uint32_t v = u32_from_word(out);
//     // Correct behavior: branch taken => SW flushed => memory stays 0
//     // Bug behavior: branch not taken => SW executed => memory becomes 1
//     assert(v == 0u);
//
//     printf("test_beq_forwarding passed!\n");
//     return 0;
// }
