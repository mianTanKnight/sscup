//
// Created by wenshen on 2025/12/5.
//

#ifndef SCCPU_PC__H
#define SCCPU_PC__H
#include "mux.h"
#include "alu.h"
#include "reg.h"

const static bit
WORD_4_BYTE[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0
};

typedef struct pc32 {
    Reg32_ reg32;
} Pc32_;


static inline void init_pc32(Pc32_ *pc32) {
    init_reg32(&pc32->reg32);
}

/**
 * 所有的模拟元件　都要遵守全局时钟的　two stage clk -> 0 , clk ->1
 * reset = 1 -> reset to init v
 * branch_taken = 1 -> set pc's v to branch_target , or else  plus 4 (4_byte-> 32bit )
 * clk -> tow stage clk Signal
 */
static inline void
pc32_word_step(Pc32_ *pc, const bit reset, const word init, const bit branch_taken, const word branch_target,
               word pc_out_,
               bit *overflow, const bit clk) {
    word pc_o_v, pc_n_v;
    read_reg32(&pc->reg32, pc_o_v);
    // ALU Add
    word_alu_(pc_o_v, WORD_4_BYTE, pc_n_v, OPS_ADD_, overflow);
    // Taken Mux
    word_mux_2_1(pc_n_v, branch_target, branch_taken, pc_n_v);
    // Reset Mux
    word_mux_2_1(pc_n_v, init, reset, pc_n_v);
    //
    reg32_step(&pc->reg32, 1, pc_n_v, pc_out_, clk);
}


#endif //SCCPU_PC__H
