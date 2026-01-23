//
// Created by wenshen on 2025/12/8.
//

#ifndef SCCPU_ISA__H
#define SCCPU_ISA__H
#include "common.h"
#include "gate.h"
/**
 * Basic Specs
 *
 * Instruction len : 32 (4 byte)
 * Addressing mode : Byte Addressing
 * PC :
 *    32-bit
 *    GPR -> R0,R1,R2,R3
 *    regIndex -> 2-bit
 *    MIPS 标准是 5-bit (32个寄存器)，我们先用 2-bit，剩下 3-bit 预留或置0
 *
 * 1: R-Type
 * Reg = Reg op Reg (如 ADD, SUB, SLT, AND, OR)
 * [31:26]  [25:21]  [20:16]  [15:11]  [10:6]  [5:0]
 * Opcode   RS       RT       RD       Shamt   Funct
 * 6bits    5bits    5bits    5bits    5bits   6bits
 * Opcode: 对于所有 R-Type，设为 000000。
 * RS, RT, RD:
 * 寄存器编码约定：
 *   逻辑寄存器号 0..3 → 物理编码 = 000xx，其中 xx = reg_id[1:0]
 *   例如：
 *     R0 → 00000
 *     R1 → 00001
 *     R2 → 00010
 *     R3 → 00011
 * 其他编码（00100..11111）暂时视为非法或保留。
 * Funct: 用来区分具体操作（ADD vs SUB vs AND...）。
 *
 * 2: I-Type
 * Reg = Reg op Imm (ADDI), Load/Store (LW, SW), Branch (BEQ)
 * [31:26]  [25:21]  [20:16]  [15:0]
 * Opcode    RS       RT       Immediate
 * 6bits     5bits    5bits     16bits
 *
 * Opcode: 具体的指令码（如 ADDI, LW）。
 * RT:
 *    对于 ADDI/LW，它是接收结果的目标寄存器。
 *    对于 SW/BEQ，它是第二个源寄存器。
 * Immediate: 16位立即数。
 *
 * 3: J-Type
 * GOTO Target(J)
 * [31:26]  [25:0]
 * Opcode   Address
 * 6bits    26bits
 *
 * Instruction Of Menu
 * R-Type  (Opcode = 000000)
 * Instruction   Asm            Funct[5:0]   Meaning
 * ADD           ADD R1,R2,R3   100000(32)   R1 = R2 + R3
 * SUB           SUB R1,R2,R3   100010(34)   R1 = R2 - R3
 * AND           AND R1,R2,R3   100100(36)   R1 = R2 & R3
 * OR            OR  R1,R2,R3   100101(37)   R1 = R2 | R3
 * SLT           SLT R1,R2,R3   101010(42)   R1 = (R2 < R3) ? 1 : 0
 *
 * I-Type  (Opcode != 000000)
 * Instruction   Asm            Opcode [31:26]   Meaning
 * LW           LW R1, 100(R2)	100011 (35)	     R1 = MEM[R2 + 100]
 * SW           SW R1, 100(R2)	101011 (43)	     MEM[R2 + 100] = R1
 * BEQ          BEQ R1, R2, 2	000100 (4)	     if(R1==R2) PC = PC+4+2*4
 * ADDI         ADDI R1, R2, 1	001000 (8)	     R1 = R2 + 1
 * Immediate:
 *   - 16-bit，默认采用有符号扩展（sign-extend）到 32-bit。
 *   - 对于 BEQ，实际跳转地址 = PC + 4 + (sign_extend(Imm) << 2)。
 * J-Type
 * Instruction   Asm            Opcode [31:26]   Meaning
 *  J            J 1000	        000010 (2)	     PC = (PC+4)[31:28] | (Addr<<2)
 */
// Opcodes (6 bits)
#define OP_R_TYPE 0b000000
#define OP_LW     0b100011
#define OP_SW     0b101011
#define OP_BEQ    0b000100
#define OP_ADDI   0b001000
#define OP_J      0b000010

// Funct codes for R-Type (6 bits)
#define FUNCT_ADD 0b100000
#define FUNCT_SUB 0b100010
#define FUNCT_AND 0b100100
#define FUNCT_OR  0b100101
#define FUNCT_SLT 0b101010


//000000 (R-Type)
static inline bit opcode6_r_type(word instruction) {
    return AND(
        AND(AND(AND(AND(NOT(INST_BIT(instruction, 31)),
                        NOT(INST_BIT(instruction, 30))),
                    NOT(INST_BIT(instruction, 29))),
                NOT(INST_BIT(instruction, 28))),
            NOT(INST_BIT(instruction, 27))),
        NOT(INST_BIT(instruction, 26)));
}

//100011 (LW)
static inline bit opcode6_op_lw(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 31),
                        NOT(INST_BIT(instruction, 30))),
                    NOT(INST_BIT(instruction, 29))),
                NOT(INST_BIT(instruction, 28))),
            INST_BIT(instruction, 27)),
        INST_BIT(instruction, 26));
}

//101011 (SW)
static inline bit opcode6_op_sw(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 31),
                        NOT(INST_BIT(instruction, 30))),
                    INST_BIT(instruction, 29)),
                NOT(INST_BIT(instruction, 28))),
            INST_BIT(instruction, 27)),
        INST_BIT(instruction, 26));
}

//000100 (BEQ)
static inline bit opcode6_op_beq(word instruction) {
    return AND(
        AND(AND(AND(AND(NOT(INST_BIT(instruction, 31)),
                        NOT(INST_BIT(instruction, 30))),
                    NOT(INST_BIT(instruction, 29))),
                INST_BIT(instruction, 28)),
            NOT(INST_BIT(instruction, 27))),
        NOT(INST_BIT(instruction, 26)));
}

//001000 (ADDI)
static inline bit opcode6_op_addi(word instruction) {
    return AND(
        AND(AND(AND(AND(NOT(INST_BIT(instruction, 31)),
                        NOT(INST_BIT(instruction, 30))),
                    INST_BIT(instruction, 29)),
                NOT(INST_BIT(instruction, 28))),
            NOT(INST_BIT(instruction, 27))),
        NOT(INST_BIT(instruction, 26)));
}

//000010 (J)
static inline bit opcode6_op_j(word instruction) {
    return AND(
        AND(AND(AND(AND(NOT(INST_BIT(instruction, 31)),
                        NOT(INST_BIT(instruction, 30))),
                    NOT(INST_BIT(instruction, 29))),
                NOT(INST_BIT(instruction, 28))),
            INST_BIT(instruction, 27)),
        NOT(INST_BIT(instruction, 26)));
}

//100000 (ADD)
static inline bit func6_add(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 5),
                        NOT(INST_BIT(instruction, 4))),
                    NOT(INST_BIT(instruction, 3))),
                NOT(INST_BIT(instruction, 2))),
            NOT(INST_BIT(instruction, 1))),
        NOT(INST_BIT(instruction, 0)));
}

//100010 (SUB)
static inline bit func6_sub(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 5),
                        NOT(INST_BIT(instruction, 4))),
                    NOT(INST_BIT(instruction, 3))),
                NOT(INST_BIT(instruction, 2))),
            INST_BIT(instruction, 1)),
        NOT(INST_BIT(instruction, 0)));
}

//100100 (AND)
static inline bit func6_and(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 5),
                        NOT(INST_BIT(instruction, 4))),
                    NOT(INST_BIT(instruction, 3))),
                INST_BIT(instruction, 2)),
            NOT(INST_BIT(instruction, 1))),
        NOT(INST_BIT(instruction, 0)));
}

//100101 (OR)
static inline bit func6_or(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 5),
                        NOT(INST_BIT(instruction, 4))),
                    NOT(INST_BIT(instruction, 3))),
                INST_BIT(instruction, 2)),
            NOT(INST_BIT(instruction, 1))),
        INST_BIT(instruction, 0));
}

//101010 (SLT)
static inline bit func6_slt(word instruction) {
    return AND(
        AND(AND(AND(AND(INST_BIT(instruction, 5),
                        NOT(INST_BIT(instruction, 4))),
                    INST_BIT(instruction, 3)),
                NOT(INST_BIT(instruction, 2))),
            INST_BIT(instruction, 1)),
        NOT(INST_BIT(instruction, 0)));
}

// any1 = OR(instruction[0..31]); nop = NOT(any1)
static inline bit nop_(const word instruction) {
    bit any1 = 0;
    for (int i = 0; i < WORD_SIZE; ++i) {
        any1 = OR(any1, instruction[i]);
    }
    return NOT(any1);
}


#endif //SCCPU_ISA__H
