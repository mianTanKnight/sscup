//
// Created by wenshen on 2026/1/22.
//

#ifndef SCCPU_UTILS_H
#define SCCPU_UTILS_H
#include "stdint.h"
#include "stdio.h"
#include "reg.h"
#include "isa.h"
#include "common.h"

// word[0]=MSB
static inline void word_lshift2(const word in, word out) {
    // out = in << 2（按你 word[0]=MSB 的约定）
    // 逻辑 bit n(31..0) -> 存储 index = 31-n
    for (int n = 31; n >= 2; --n) {
        out[INST_WORD(n)] = in[INST_WORD(n - 2)];
    }
    out[INST_WORD(1)] = 0;
    out[INST_WORD(0)] = 0;
}

static inline
uint8_t u8_from_byte(const byte b) {
    uint8_t v = 0;
    for (int i = 0; i < BYTE_SIZE; i++) {
        if (b[i]) {
            v |= (uint8_t) (1u << (BYTE_SIZE - 1 - i));
        }
    }
    return v;
}


static inline
void u32_from_4byte(const word w, uint8_t *ret) {
    for (int i = 0; i < 4; i++) {
        int v = 0;
        for (int j = 0; j < BYTE_SIZE; j++) {
            if (w[i * BYTE_SIZE + j]) {
                v |= (uint8_t) (1u << (BYTE_SIZE - 1 - j));
            }
        }
        ret[i] = v;
    }
}


static inline
void connect(const word src, word dest) {
    for (int i = 0; i < WORD_SIZE; i++) {
        dest[i] = src[i];
    }
}

static inline
uint32_t u32_from_word(const word b) {
    uint32_t v = 0;
    for (int i = 0; i < WORD_SIZE; i++) {
        if (b[i]) {
            v |= (uint32_t) (1u << (WORD_SIZE - 1 - i));
        }
    }
    return v;
}

static inline
bit word_is_zero(const word b) {
    bit mask = 0;
    for (int i = 0; i < BYTE_SIZE; i++) {
        mask |= b[i];
    }
    return !mask;
}

static inline
uint32_t reg32_read_u32_(const Reg32_ *rp) {
    word w = {0};
    read_reg32(rp, w);
    return u32_from_word(w);
}


static inline void dis_asm(const uint32_t inst, char *buffer) {
    const uint32_t op = (inst >> 26) & 0x3F;
    const uint32_t rs = (inst >> 21) & 0x1F; //只用低2位，但解码看5位
    const uint32_t rt = (inst >> 16) & 0x1F;
    const uint32_t rd = (inst >> 11) & 0x1F;
    const uint32_t funct = inst & 0x3F;
    const int16_t imm = (int16_t) (inst & 0xFFFF); //强转为有符号数
    const uint32_t addr = inst & 0x03FFFFFF;
    // 2. NOP 特殊处理
    if (inst == 0) {
        sprintf(buffer, "NOP");
        return;
    }
    // 3. 根据 Opcode 格式化字符串
    switch (op) {
        case OP_R_TYPE: // 0x00
            switch (funct) {
                // R-Type 格式: OP RD, RS, RT
                case FUNCT_ADD: sprintf(buffer, "ADD  R%d, R%d, R%d", rd, rs, rt);
                    break;
                case FUNCT_SUB: sprintf(buffer, "SUB  R%d, R%d, R%d", rd, rs, rt);
                    break;
                case FUNCT_AND: sprintf(buffer, "AND  R%d, R%d, R%d", rd, rs, rt);
                    break;
                case FUNCT_OR: sprintf(buffer, "OR   R%d, R%d, R%d", rd, rs, rt);
                    break;
                case FUNCT_SLT: sprintf(buffer, "SLT  R%d, R%d, R%d", rd, rs, rt);
                    break;
                default: sprintf(buffer, "R-UNK (Funct:0x%02X)", funct);
                    break;
            }
            break;
        // I-Type 计算: OP RT, RS, IMM
        case OP_ADDI: sprintf(buffer, "ADDI R%d, R%d, %d", rt, rs, imm);
            break;
        // I-Type 访存: OP RT, IMM(RS)
        case OP_LW: sprintf(buffer, "LW   R%d, %d(R%d)", rt, imm, rs);
            break;
        case OP_SW: sprintf(buffer, "SW   R%d, %d(R%d)", rt, imm, rs);
            break;
        // I-Type 分支: OP RS, RT, IMM
        case OP_BEQ: sprintf(buffer, "BEQ  R%d, R%d, %d", rs, rt, imm);
            break;
        // J-Type 跳转: OP ADDR
        case OP_J: sprintf(buffer, "J    0x%07X", addr);
            break;

        default:
            sprintf(buffer, "UNK  (Op:0x%02X)", op);
            break;
    }
}

#endif //SCCPU_UTILS_H
