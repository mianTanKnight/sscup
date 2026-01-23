//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_ALU__H
#define SCCPU_ALU__H

#include "common.h"
#include "gate.h"
#include "mux.h"


/**
 * Opcode  功能	 分组 (Op[2])	 备注
 * 000     AND    Logic(0)       选 AND
 * 001     OR     Logic(0)       选  OR
 * 010     XOR    Logic(0)       选  XOR
 * 011     NOR    Logic(0)       选  NOR
 * 100     ADD    Arith(1)       加法器输出
 * 101     SUB    Arith(1)       加法器输出 Cin
 * 110     SLT    Arith(1)       加法器输出 (但是要做特殊的 SLT 处理)
 * 111     NULL   Arith(1)       0
 */
typedef bit ops[3];
static const ops OPS_ADD_ = {1, 0, 0};
static const  ops OPS_SUB_ = {1, 0, 1};
static const ops OPS_SLT_ = {1, 1, 0};
static const ops OPS_NULL_ = {1, 1, 1};

static const ops OPS_AND_ = {0, 0, 0};
static const ops OPS_OR_ = {0, 0, 1};
static const ops OPS_XOR_ = {0, 1, 0};
static const ops OPS_NOR_ = {0, 1, 1};


static inline bit full_adder(const bit input1, const bit input2, const bit cin, bit *cout) {
    const bit ret_xor = XOR(input1, input2);
    const bit ret_xor_1 = XOR(ret_xor, cin);
    *cout = OR(AND(input1, input2), AND(ret_xor, cin));
    return ret_xor_1;
}

static inline bit one_bit_alu_(const bit input0, const bit input1, const bit cin, const ops ops_,
                               bit *cout) {
    bit add_cout_v = *cout;
    bit sub_cout_v = *cout;

    bit ADD_RET = full_adder(input0, input1, cin, &add_cout_v);
    //  1: cin 在 index[0] 初始化 是 1
    //  2: SLT 在 bit_alu 也使用的是 SUB
    bit SUB_RET = full_adder(input0, NOT(input1), cin, &sub_cout_v);
    // NULL Is 0
    bit NULL_RET = 0;
    bit AND_RET = AND(input0, input1);
    bit OR_RET = OR(input0, input1);
    bit XOR_RET = XOR(input0, input1);
    bit NOR_RET = NOR(input0, input1);

    // Multiplexer Of Group
    bit group_0_sel_0 = mux2_1(XOR_RET, SUB_RET, ops_[0]);
    bit group_0_sel_1 = mux2_1(NOR_RET, NULL_RET, ops_[0]);
    bit group_0_sel_2 = mux2_1(AND_RET, ADD_RET, ops_[0]);
    bit group_0_sel_3 = mux2_1(OR_RET, SUB_RET, ops_[0]);

    bit group_1_sel_0 = mux2_1(group_0_sel_2, group_0_sel_0, ops_[1]);
    bit group_1_sel_1 = mux2_1(group_0_sel_3, group_0_sel_1, ops_[1]);

    bit select = mux2_1(group_1_sel_0, group_1_sel_1, ops_[2]);

    //只有 ADD和 SUB 才有进位
    bit cout_add = AND(AND(AND(ops_[0], NOT(ops_[1])), NOT(ops_[2])), add_cout_v);
    bit cout_sub = AND(AND(AND(ops_[0], NOT(ops_[1])), ops_[2]), sub_cout_v);
    bit cout_slt = AND(AND(AND(ops_[0], ops_[1]), NOT(ops_[2])), sub_cout_v);
    *cout = OR(OR(cout_add, cout_sub), cout_slt);
    return select;
}

static inline void byte_alu_(const byte input0, const byte input1, byte ret, const ops ops_, bit *overflow) {
    bit is_op_sub = AND(AND(ops_[0], NOT(ops_[1])), ops_[2]);
    bit is_op_slt = AND(AND(ops_[0], ops_[1]), NOT(ops_[2]));
    bit cin = OR(is_op_sub, is_op_slt);
    bit carry_into_msb = 0;

    for (int i = BYTE_SIZE - 1; i > 0; --i) {
        ret[i] = one_bit_alu_(input0[i], input1[i], cin, ops_, &cin);
    }
    carry_into_msb = cin;
    ret[0] = one_bit_alu_(input0[0], input1[0], cin, ops_, &cin);

    bit true_overflow = XOR(carry_into_msb, cin);
    bit less = XOR(ret[0], true_overflow);
    for (int i = 0; i < BYTE_SIZE - 1; ++i) {
        ret[i] = mux2_1(ret[i], 0, is_op_slt);
    }
    ret[BYTE_SIZE - 1] = mux2_1(ret[BYTE_SIZE - 1], less, is_op_slt);
    *overflow = cin;
}


static inline void word_alu_(const word input0, const word input1, word ret, const ops ops_, bit *overflow) {
    bit is_op_sub = AND(AND(ops_[0], NOT(ops_[1])), ops_[2]);
    bit is_op_slt = AND(AND(ops_[0], ops_[1]), NOT(ops_[2]));
    bit cin = OR(is_op_sub, is_op_slt);
    bit carry_into_msb = 0;

    for (int i = WORD_SIZE - 1; i > 0; --i) {
        ret[i] = one_bit_alu_(input0[i], input1[i], cin, ops_, &cin);
    }
    carry_into_msb = cin;
    ret[0] = one_bit_alu_(input0[0], input1[0], cin, ops_, &cin);

    bit true_overflow = XOR(carry_into_msb, cin);
    bit less = XOR(ret[0], true_overflow);
    for (int i = 0; i < WORD_SIZE - 1; ++i) {
        ret[i] = mux2_1(ret[i], 0, is_op_slt);
    }
    ret[WORD_SIZE - 1] = mux2_1(ret[WORD_SIZE - 1], less, is_op_slt);
    *overflow = cin;
}


#endif //SCCPU_ALU__H
