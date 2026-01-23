//
// Created by wenshen on 2025/12/12.
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../includes/common.h"
#include "../includes/alu.h"
#include "../includes/isa.h"
#include "../includes/decoder.h"

// ------------------------------------------------------------
// 位序约定：word[0]=bit31 ... word[31]=bit0
// ------------------------------------------------------------
#ifndef INST_BIT
#define INST_BIT(instr, n) ((instr)[31 - (n)])  // n 是“位编号”31..0
#endif
#include "common_test.h"

// ------------------------------------------------------------
// 指令编码（用 u32 构造，再转 word）
// reg 只有 2-bit，但字段是 5-bit：只用低2位，高3位=0
// ------------------------------------------------------------
static inline uint32_t enc_reg5(uint32_t r2bit) {
    return (r2bit & 0x3u); // 000xx
}

static inline void make_r(word out, uint32_t rs2, uint32_t rt2, uint32_t rd2,
                          uint32_t shamt, uint32_t funct) {
    uint32_t opcode = 0u;
    uint32_t rs = enc_reg5(rs2);
    uint32_t rt = enc_reg5(rt2);
    uint32_t rd = enc_reg5(rd2);
    uint32_t inst =
            (opcode << 26) |
            (rs << 21) |
            (rt << 16) |
            (rd << 11) |
            ((shamt & 0x1Fu) << 6) |
            (funct & 0x3Fu);
    word_from_u32(inst, out);
}

static inline void make_i(word out, uint32_t opcode6, uint32_t rs2, uint32_t rt2, int16_t imm) {
    uint32_t rs = enc_reg5(rs2);
    uint32_t rt = enc_reg5(rt2);
    uint32_t inst =
            ((opcode6 & 0x3Fu) << 26) |
            (rs << 21) |
            (rt << 16) |
            ((uint16_t) imm);
    word_from_u32(inst, out);
}

static inline void make_j(word out, uint32_t opcode6, uint32_t addr26) {
    uint32_t inst =
            ((opcode6 & 0x3Fu) << 26) |
            (addr26 & 0x03FFFFFFu);
    word_from_u32(inst, out);
}

// ------------------------------------------------------------
// 断言工具
// ------------------------------------------------------------
#define PASS(name) printf("[PASS] %s\n", (name))
#define FAIL(name, fmt, ...) do { printf("[FAIL] %s: " fmt "\n", (name), ##__VA_ARGS__); } while (0)

static inline int eq_ops(const ops a, const ops b) {
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}

static inline void assert_bit(const char *name, bit got, bit exp) {
    if (((got) & 1) != ((exp) & 1))
        FAIL(name, "got=%d exp=%d", (int)got, (int)exp);
    else
        PASS(name);
}

static inline void assert_ops(const char *name, const ops got, const ops exp) {
    if (!eq_ops(got, exp)) {
        FAIL(name, "got=[%d%d%d] exp=[%d%d%d]",
             got[0], got[1], got[2], exp[0], exp[1], exp[2]);
    } else
        PASS(name);
}

static inline void assert_cs_common(const char *prefix, const Control_signals cs,
                                    bit reg_dst, bit alu_src, bit mem_to_reg,
                                    bit reg_write, bit mem_read, bit mem_write,
                                    bit branch, bit jump,
                                    const ops *exp_ops_or_null) {
    char buf[128];

    snprintf(buf, sizeof(buf), "%s.reg_dst", prefix);
    assert_bit(buf, cs.reg_dst, reg_dst);
    snprintf(buf, sizeof(buf), "%s.alu_src", prefix);
    assert_bit(buf, cs.alu_src, alu_src);
    snprintf(buf, sizeof(buf), "%s.mem_to_reg", prefix);
    assert_bit(buf, cs.data_src_to_reg, mem_to_reg);

    snprintf(buf, sizeof(buf), "%s.reg_write", prefix);
    assert_bit(buf, cs.reg_write, reg_write);
    snprintf(buf, sizeof(buf), "%s.mem_read", prefix);
    assert_bit(buf, cs.mem_read, mem_read);
    snprintf(buf, sizeof(buf), "%s.mem_write", prefix);
    assert_bit(buf, cs.mem_write, mem_write);

    snprintf(buf, sizeof(buf), "%s.branch", prefix);
    assert_bit(buf, cs.branch, branch);
    snprintf(buf, sizeof(buf), "%s.jump", prefix);
    assert_bit(buf, cs.jump, jump);

    if (exp_ops_or_null) {
        snprintf(buf, sizeof(buf), "%s.ops", prefix);
        assert_ops(buf, cs.ops_, *exp_ops_or_null);
    }
}

// ------------------------------------------------------------
// 单点测试：每条指令类型
// ------------------------------------------------------------
static void test_decode_rtype(void) {
    printf("\n=== test_decode_rtype ===\n");
    word inst;

    make_r(inst, 2, 3, 1, 0, FUNCT_ADD);
    assert_cs_common("R/ADD", decode(inst), 1, 0, 0, 1, 0, 0, 0, 0, &OPS_ADD_);

    make_r(inst, 2, 3, 1, 0, FUNCT_SUB);
    assert_cs_common("R/SUB", decode(inst), 1, 0, 0, 1, 0, 0, 0, 0, &OPS_SUB_);

    make_r(inst, 2, 3, 1, 0, FUNCT_AND);
    assert_cs_common("R/AND", decode(inst), 1, 0, 0, 1, 0, 0, 0, 0, &OPS_AND_);

    make_r(inst, 2, 3, 1, 0, FUNCT_OR);
    assert_cs_common("R/OR", decode(inst), 1, 0, 0, 1, 0, 0, 0, 0, &OPS_OR_);

    make_r(inst, 2, 3, 1, 0, FUNCT_SLT);
    assert_cs_common("R/SLT", decode(inst), 1, 0, 0, 1, 0, 0, 0, 0, &OPS_SLT_);
}

static void test_decode_itype(void) {
    printf("\n=== test_decode_itype ===\n");
    word inst;

    make_i(inst, OP_LW, 2, 1, 123);
    assert_cs_common("I/LW", decode(inst), 0, 1, 1, 1, 1, 0, 0, 0, &OPS_ADD_);

    make_i(inst, OP_SW, 2, 1, -4);
    assert_cs_common("I/SW", decode(inst), 0, 1, 0, 0, 0, 1, 0, 0, &OPS_ADD_);

    make_i(inst, OP_ADDI, 2, 1, 1);
    assert_cs_common("I/ADDI", decode(inst), 0, 1, 0, 1, 0, 0, 0, 0, &OPS_ADD_);

    make_i(inst, OP_BEQ, 1, 2, 7);
    assert_cs_common("I/BEQ", decode(inst), 0, 0, 0, 0, 0, 0, 1, 0, &OPS_SUB_);
}

static void test_decode_jtype(void) {
    printf("\n=== test_decode_jtype ===\n");
    word inst;
    make_j(inst, OP_J, 0x123456);

    Control_signals cs = decode(inst);
    assert_bit("J.jump", cs.jump, 1);
    assert_bit("J.reg_write", cs.reg_write, 0);
    assert_bit("J.mem_read", cs.mem_read, 0);
    assert_bit("J.mem_write", cs.mem_write, 0);
    assert_bit("J.branch", cs.branch, 0);
}

// ------------------------------------------------------------
// 随机回归：不变量
// ------------------------------------------------------------
static uint32_t lcg_next(uint32_t *s) {
    *s = (*s) * 1664525u + 1013904223u;
    return *s;
}

static void test_decode_invariants_random(void) {
    printf("\n=== test_decode_invariants_random ===\n");
    uint32_t s = 0xC0FFEEu;

    for (int t = 0; t < 5000; t++) {
        uint32_t r = lcg_next(&s);
        uint32_t pick = (r >> 28) & 0xF;

        word inst;
        word_zero(inst);

        if (pick < 5) {
            uint32_t functs[5] = {FUNCT_ADD, FUNCT_SUB, FUNCT_AND, FUNCT_OR, FUNCT_SLT};
            uint32_t f = functs[pick];
            make_r(inst, lcg_next(&s) & 3, lcg_next(&s) & 3, lcg_next(&s) & 3, 0, f);
        } else if (pick < 9) {
            uint32_t ops6[4] = {OP_LW, OP_SW, OP_ADDI, OP_BEQ};
            uint32_t op = ops6[pick - 5];
            make_i(inst, op, lcg_next(&s) & 3, lcg_next(&s) & 3, (int16_t) (lcg_next(&s) & 0xFFFF));
        } else {
            make_j(inst, OP_J, lcg_next(&s) & 0x03FFFFFFu);
        }

        Control_signals cs = decode(inst);

        if (cs.mem_read && cs.mem_write) {
            FAIL("INV mem_read && mem_write", "inst=0x%08X", u32_from_word_test(inst));
            return;
        }
        if (cs.branch && cs.jump) {
            FAIL("INV branch && jump", "inst=0x%08X", u32_from_word_test(inst));
            return;
        }
        if (cs.mem_read) {
            if (!cs.reg_write || !cs.data_src_to_reg) {
                FAIL("INV LW shape", "inst=0x%08X", u32_from_word_test(inst));
                return;
            }
        }
        if (cs.mem_write) {
            if (cs.reg_write) {
                FAIL("INV SW shape", "inst=0x%08X", u32_from_word_test(inst));
                return;
            }
        }
        if (cs.branch) {
            if (!eq_ops(cs.ops_, OPS_SUB_)) {
                FAIL("INV BEQ uses SUB", "inst=0x%08X got_ops=[%d%d%d]",
                     u32_from_word_test(inst), cs.ops_[0], cs.ops_[1], cs.ops_[2]);
                return;
            }
        }
    }

    PASS("random invariants 5000 cases");
}

// ------------------------------------------------------------
// 质量观察：非法 funct
// ------------------------------------------------------------
static void test_illegal_rtype_funct_behavior(void) {
    printf("\n=== test_illegal_rtype_funct_behavior ===\n");
    word inst;
    make_r(inst, 0, 1, 2, 0, 0x3F);

    Control_signals cs = decode(inst);
    printf("illegal R-type funct: reg_write=%d, ops=[%d%d%d], inst=0x%08X\n",
           (int) cs.reg_write, (int) cs.ops_[0], (int) cs.ops_[1], (int) cs.ops_[2],
           u32_from_word_test(inst));
    PASS("printed current behavior");
}

// int main(void) {
//     test_decode_rtype();
//     test_decode_itype();
//     test_decode_jtype();
//     test_decode_invariants_random();
//     test_illegal_rtype_funct_behavior();
//     return 0;
// }
