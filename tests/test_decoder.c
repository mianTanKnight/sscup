// test_decoder.c
// Decoder unit tests: R-type, I-type, J-type single-point + 5000 random invariants

#include <stdio.h>
#include <stdint.h>
#include "../includes/decoder.h"
#include "common_test.h"

// ======================== Instruction Encoding Helpers ========================

static inline uint32_t enc_reg5(uint32_t r2bit) {
    return (r2bit & 0x3u);
}

static inline void make_r(word out, uint32_t rs2, uint32_t rt2, uint32_t rd2,
                          uint32_t shamt, uint32_t funct) {
    uint32_t inst =
            (0u << 26) |
            (enc_reg5(rs2) << 21) |
            (enc_reg5(rt2) << 16) |
            (enc_reg5(rd2) << 11) |
            ((shamt & 0x1Fu) << 6) |
            (funct & 0x3Fu);
    word_from_u32(inst, out);
}

static inline void make_i(word out, uint32_t opcode6, uint32_t rs2, uint32_t rt2, int16_t imm) {
    uint32_t inst =
            ((opcode6 & 0x3Fu) << 26) |
            (enc_reg5(rs2) << 21) |
            (enc_reg5(rt2) << 16) |
            ((uint16_t)imm);
    word_from_u32(inst, out);
}

static inline void make_j(word out, uint32_t opcode6, uint32_t addr26) {
    uint32_t inst =
            ((opcode6 & 0x3Fu) << 26) |
            (addr26 & 0x03FFFFFFu);
    word_from_u32(inst, out);
}

// ======================== Assert Helpers for Control Signals ========================

static inline int eq_ops(const ops a, const ops b) {
    return (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}

static inline void assert_bit_local(const char *name, bit got, bit exp) {
    if (((got) & 1) != ((exp) & 1))
        printf("[FAIL] %s: got=%d exp=%d\n", name, (int)got, (int)exp);
    else
        printf("[PASS] %s\n", name);
}

static inline void assert_ops_local(const char *name, const ops got, const ops exp) {
    if (!eq_ops(got, exp))
        printf("[FAIL] %s: got=[%d%d%d] exp=[%d%d%d]\n",
               name, got[0], got[1], got[2], exp[0], exp[1], exp[2]);
    else
        printf("[PASS] %s\n", name);
}

static inline void assert_cs_common(const char *prefix, const Control_signals cs,
                                    bit reg_dst, bit alu_src, bit mem_to_reg,
                                    bit reg_write, bit mem_read, bit mem_write,
                                    bit branch, bit jump,
                                    const ops *exp_ops_or_null) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s.reg_dst", prefix);
    assert_bit_local(buf, cs.reg_dst, reg_dst);
    snprintf(buf, sizeof(buf), "%s.alu_src", prefix);
    assert_bit_local(buf, cs.alu_src, alu_src);
    snprintf(buf, sizeof(buf), "%s.mem_to_reg", prefix);
    assert_bit_local(buf, cs.data_src_to_reg, mem_to_reg);
    snprintf(buf, sizeof(buf), "%s.reg_write", prefix);
    assert_bit_local(buf, cs.reg_write, reg_write);
    snprintf(buf, sizeof(buf), "%s.mem_read", prefix);
    assert_bit_local(buf, cs.mem_read, mem_read);
    snprintf(buf, sizeof(buf), "%s.mem_write", prefix);
    assert_bit_local(buf, cs.mem_write, mem_write);
    snprintf(buf, sizeof(buf), "%s.branch", prefix);
    assert_bit_local(buf, cs.branch, branch);
    snprintf(buf, sizeof(buf), "%s.jump", prefix);
    assert_bit_local(buf, cs.jump, jump);
    if (exp_ops_or_null) {
        snprintf(buf, sizeof(buf), "%s.ops", prefix);
        assert_ops_local(buf, cs.ops_, *exp_ops_or_null);
    }
}

// ======================== Tests ========================

static int test_decode_rtype(void) {
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

    return 0;
}

static int test_decode_itype(void) {
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

    return 0;
}

static int test_decode_jtype(void) {
    printf("\n=== test_decode_jtype ===\n");
    word inst;
    make_j(inst, OP_J, 0x123456);

    Control_signals cs = decode(inst);
    assert_bit_local("J.jump", cs.jump, 1);
    assert_bit_local("J.reg_write", cs.reg_write, 0);
    assert_bit_local("J.mem_read", cs.mem_read, 0);
    assert_bit_local("J.mem_write", cs.mem_write, 0);
    assert_bit_local("J.branch", cs.branch, 0);

    return 0;
}

// ======================== Random Invariant Tests ========================

static uint32_t lcg_next(uint32_t *s) {
    *s = (*s) * 1664525u + 1013904223u;
    return *s;
}

static int test_decode_invariants_random(void) {
    printf("\n=== test_decode_invariants_random (5000 cases) ===\n");
    uint32_t s = 0xC0FFEEu;

    for (int t = 0; t < 5000; t++) {
        uint32_t r = lcg_next(&s);
        uint32_t pick = (r >> 28) & 0xF;

        word inst = {0};

        if (pick < 5) {
            uint32_t functs[5] = {FUNCT_ADD, FUNCT_SUB, FUNCT_AND, FUNCT_OR, FUNCT_SLT};
            make_r(inst, lcg_next(&s) & 3, lcg_next(&s) & 3, lcg_next(&s) & 3, 0, functs[pick]);
        } else if (pick < 9) {
            uint32_t ops6[4] = {OP_LW, OP_SW, OP_ADDI, OP_BEQ};
            make_i(inst, ops6[pick - 5], lcg_next(&s) & 3, lcg_next(&s) & 3, (int16_t)(lcg_next(&s) & 0xFFFF));
        } else {
            make_j(inst, OP_J, lcg_next(&s) & 0x03FFFFFFu);
        }

        Control_signals cs = decode(inst);

        if (cs.mem_read && cs.mem_write) {
            printf("[FAIL] INV mem_read && mem_write: inst=0x%08X\n", word_to_u32(inst));
            return 1;
        }
        if (cs.branch && cs.jump) {
            printf("[FAIL] INV branch && jump: inst=0x%08X\n", word_to_u32(inst));
            return 1;
        }
        if (cs.mem_read) {
            if (!cs.reg_write || !cs.data_src_to_reg) {
                printf("[FAIL] INV LW shape: inst=0x%08X\n", word_to_u32(inst));
                return 1;
            }
        }
        if (cs.mem_write) {
            if (cs.reg_write) {
                printf("[FAIL] INV SW shape: inst=0x%08X\n", word_to_u32(inst));
                return 1;
            }
        }
        if (cs.branch) {
            if (!eq_ops(cs.ops_, OPS_SUB_)) {
                printf("[FAIL] INV BEQ uses SUB: inst=0x%08X\n", word_to_u32(inst));
                return 1;
            }
        }
    }

    PASS("random invariants 5000 cases");
    return 0;
}

static int test_illegal_rtype_funct_behavior(void) {
    printf("\n=== test_illegal_rtype_funct_behavior ===\n");
    word inst;
    make_r(inst, 0, 1, 2, 0, 0x3F);

    Control_signals cs = decode(inst);
    printf("illegal R-type funct: reg_write=%d, ops=[%d%d%d], inst=0x%08X\n",
           (int)cs.reg_write, (int)cs.ops_[0], (int)cs.ops_[1], (int)cs.ops_[2],
           word_to_u32(inst));
    PASS("printed current behavior");
    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_decode_rtype);
    RUN_TEST(test_decode_itype);
    RUN_TEST(test_decode_jtype);
    RUN_TEST(test_decode_invariants_random);
    RUN_TEST(test_illegal_rtype_funct_behavior);
    TEST_SUMMARY("Decoder Tests");
    return rc;
}
