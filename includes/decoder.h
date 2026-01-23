//
// Created by wenshen on 2025/12/9.
//

#ifndef SCCPU_DECODER__H
#define SCCPU_DECODER__H
#include "alu.h"
#include "isa.h"

// info see @isa_.h

typedef struct {
    /**
     * R1 = R2 + R3
     * RT是 Instruction 中指定的bit位置信息
     * [25:21]  [20:16]  [15:11]
     * RS       RT       RD
     * 写目的地reg 到底是 RT(0) 还是 RD(1)
     * RS 永远是 read0
     *
     */
    bit reg_dst;

    /**
     *  RS 永远是 read0(input 0)
     *  read1(input 1) 到底是 RT(0) 还是 Imm(1)
     */
    bit alu_src;

    /**
     * 写回到 目的地寄存器的数据来源是 ALU(0) 还是 MEM(1)
     */
    bit data_src_to_reg;

    /**
     *  RegFile 写使能
     */
    bit reg_write;

    /**
     *  Mem 读使能
     */
    bit mem_read;

    /**
     * Mem 写使能
     */
    bit mem_write;

    /**
     *  是否 BEQ
     */
    bit branch;

    /**
     * 是否 jump
     */
    bit jump;

    /**
     * ALU ops
     */
    ops ops_;
}  Control_signals;


static inline Control_signals decode(word instruction) {
    Control_signals cs = {0};
    //op_codes
    const bit op_r_type = opcode6_r_type(instruction);
    const bit op_sw = opcode6_op_sw(instruction);
    const bit op_lw = opcode6_op_lw(instruction);
    const bit op_addi = opcode6_op_addi(instruction);
    const bit op_beq = opcode6_op_beq(instruction);
    const bit op_j = opcode6_op_j(instruction);

    ops add_, and_, or_, sub_, slt_;
    const bit func_add = AND(op_r_type, func6_add(instruction));
    add_[0] = AND(func_add, OPS_ADD_[0]);
    add_[1] = AND(func_add, OPS_ADD_[1]);
    add_[2] = AND(func_add, OPS_ADD_[2]);

    const bit func_and = AND(op_r_type, func6_and(instruction));
    and_[0] = AND(func_and, OPS_AND_[0]);
    and_[1] = AND(func_and, OPS_AND_[1]);
    and_[2] = AND(func_and, OPS_AND_[2]);

    const bit func_or = AND(op_r_type, func6_or(instruction));
    or_[0] = AND(func_or, OPS_OR_[0]);
    or_[1] = AND(func_or, OPS_OR_[1]);
    or_[2] = AND(func_or, OPS_OR_[2]);

    const bit func_sub = AND(op_r_type, func6_sub(instruction));
    sub_[0] = AND(func_sub, OPS_SUB_[0]);
    sub_[1] = AND(func_sub, OPS_SUB_[1]);
    sub_[2] = AND(func_sub, OPS_SUB_[2]);

    const bit func_slt = AND(op_r_type, func6_slt(instruction));
    slt_[0] = AND(func_slt, OPS_SLT_[0]);
    slt_[1] = AND(func_slt, OPS_SLT_[1]);
    slt_[2] = AND(func_slt, OPS_SLT_[2]);


    // 我们需要支持 NOP -> instruction is 0
    // 如果是 NOP 在 Decode 中 首先会击中 op_r_type
    //  func_add, func_and, func_or, func_sub, func_slt 全是 0
    //  所以 ops_ = {0,0,0} -> AND  ->  R0 = R0 & RO 无副作用
    //  但 reg_write 在  op_r_type 是 1

    const bit nop = nop_(instruction);

    // addi/lw/sw -> add
    ops add_i_type, sub_i_type;
    add_i_type[0] = AND(OR(OR(op_sw, op_lw), op_addi), OPS_ADD_[0]);
    add_i_type[1] = AND(OR(OR(op_sw, op_lw), op_addi), OPS_ADD_[1]);
    add_i_type[2] = AND(OR(OR(op_sw, op_lw), op_addi), OPS_ADD_[2]);

    //beq -> sub
    sub_i_type[0] = AND(op_beq, OPS_SUB_[0]);
    sub_i_type[1] = AND(op_beq, OPS_SUB_[1]);
    sub_i_type[2] = AND(op_beq, OPS_SUB_[2]);

    // set ops
    cs.ops_[0] = OR(OR(OR(OR(OR(add_[0], add_i_type[0]), and_[0]), or_[0]), OR(sub_[0], sub_i_type[0])), slt_[0]);
    cs.ops_[1] = OR(OR(OR(OR(OR(add_[1], add_i_type[1]), and_[1]), or_[1]), OR(sub_[1], sub_i_type[1])), slt_[1]);
    cs.ops_[2] = OR(OR(OR(OR(OR(add_[2], add_i_type[2]), and_[2]), or_[2]), OR(sub_[2], sub_i_type[2])), slt_[2]);

    // R-Type -> RD
    // I-Type -> RT
    // J-Type -> PC
    // 1 -> RD , 0 -> RT
    cs.reg_dst = op_r_type;

    // * LW           LW R1, 100(R2)	100011 (35)	     R1 = MEM[R2 + 100]
    // * SW           SW R1, 100(R2)	101011 (43)	     MEM[R2 + 100] = R1
    // * BEQ          BEQ R1, R2, 2	000100 (4)	        if(R1==R2) PC = PC+4+2*4
    //  * ADDI        ADDI R1, R2, 1	001000 (8)	     R1 = R2 + 1
    // LW,SW,ADDI 是 立即值需求 其余是 RT
    cs.alu_src = OR(OR(op_lw, op_sw), op_addi);

    // 只有lw 来至内存
    cs.data_src_to_reg = op_lw;

    // r_type , lw , addi 写 regfile
    cs.reg_write = AND(OR(OR(op_r_type, op_lw), op_addi), NOT(nop));

    cs.mem_read = op_lw;
    cs.mem_write = op_sw;
    cs.branch = op_beq;
    cs.jump = op_j;

    return cs;
}


#endif //SCCPU_DECODER__H
