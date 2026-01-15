//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_REG__H
#define SCCPU_REG__H

#include "common_.h"
#include "mux_.h"
#include "dff_.h"

typedef struct reg32_ {
    dff_b_ dffs[WORD_SIZE];
} Reg32_;

typedef struct reg324file_ {
    Reg32_ r0;
    Reg32_ r1;
    Reg32_ r2;
    Reg32_ r3;
} Reg324file_;


static inline void
init_reg32(Reg32_ *reg) {
    for (int i = 0; i < WORD_SIZE; i++)
        init_dff_deh(&reg->dffs[i]);
}

static inline void
init_reg32file(Reg324file_ *reg324) {
    init_reg32(&reg324->r0);
    init_reg32(&reg324->r1);
    init_reg32(&reg324->r2);
    init_reg32(&reg324->r3);
}

// 从寄存器内部状态读出当前 Q 到 byte 数组
static inline void
read_reg32(const Reg32_ *r, word out) {
    for (int i = 0; i < WORD_SIZE; ++i) {
        out[i] = r->dffs[i].dff.Q;
    }
}

static inline
void reg32_step(Reg32_ *reg, const bit load, const word d_in, word p_out, const bit clk) {
    for (int i = 0; i < WORD_SIZE; ++i) {
        // select
        bit r = mux2_1(reg->dffs[i].dff.Q, d_in[i], load);
        p_out[i] = dff_deh_step(&reg->dffs[i], clk, r);
    }
}

typedef struct regfile_in {
    bit we3;
    const bit *a1;
    const bit *a2;
    const bit *a3;
    word wd3;
} Regfile_in;

/**
 * CLK: 时钟信号。
 * WE3 (Write Enable): 写使能信号（为 1 时才允许修改值）。
 * 00 r0, 01 r1, 10 r2, 11 r3
 * A1 (Address 1): 2-bit, 读地址 1 (想读哪个寄存器给 ALU 的 A 口?)。
 * A2 (Address 2): 2-bit, 读地址 2 (想读哪个寄存器给 ALU 的 B 口?)。
 * A3 (Address 3): 2-bit, 写地址 (结果想存入哪个寄存器?)。
 * WD3 (Write Data): 32-bit, 想要写入的数据。
 * 输出：
 * RD1 (Read Data 1): 32-bit, 从 A1 地址读出的数据。
 * RD2 (Read Data 1): 32-bit, 从 A2 地址读出的数据。
 */
static inline void
reg324file_step(Reg324file_ *reg324, const Regfile_in *in, word rd1, word rd2, const bit clk) {
    // Electric currents are parallel
    const bit *a3_ = in->a3;

    bit reg0_e = AND(AND(NOT(a3_[0]), NOT(a3_[1])), in->we3);
    bit reg1_e = AND(AND(NOT(a3_[0]), a3_[1]), in->we3);
    bit reg2_e = AND(AND(a3_[0], NOT(a3_[1])), in->we3);
    bit reg3_e = AND(AND(a3_[0], a3_[1]), in->we3);

    word o_put;
    reg32_step(&reg324->r0, reg0_e, in->wd3, o_put, clk);
    reg32_step(&reg324->r1, reg1_e, in->wd3, o_put, clk);
    reg32_step(&reg324->r2, reg2_e, in->wd3, o_put, clk);
    reg32_step(&reg324->r3, reg3_e, in->wd3, o_put, clk);

    word r0v, r1v, r2v, r3v;

    // 00->r0, 01->r1, 10->r2, 11->r3
    read_reg32(&reg324->r0, r0v);
    read_reg32(&reg324->r1, r1v);
    read_reg32(&reg324->r2, r2v);
    read_reg32(&reg324->r3, r3v);

    // A1
    word sv1_a1;
    word_mux_2_1(r0v, r2v, in->a1[0], sv1_a1);
    word sv2_a1;
    word_mux_2_1(r1v, r3v, in->a1[0], sv2_a1);
    word sa1v;
    word_mux_2_1(sv1_a1, sv2_a1, in->a1[1], sa1v);

    // A2
    word sv1_a2;
    word_mux_2_1(r0v, r2v, in->a2[0], sv1_a2);
    word sv2_a2;
    word_mux_2_1(r1v, r3v, in->a2[0], sv2_a2);
    word sa2v;
    word_mux_2_1(sv1_a2, sv2_a2, in->a2[1], sa2v);

    for (int i = 0; i < WORD_SIZE; i++)rd1[i] = sa1v[i];
    for (int i = 0; i < WORD_SIZE; i++)rd2[i] = sa2v[i];
}


#endif //SCCPU_REG__H
