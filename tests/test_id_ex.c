// test_id_ex.c
// ID/EX pipeline register tests: imm sign-extend, control signals, reg select, stall, flush

#include <stdio.h>
#include <stdlib.h>
#include "../includes/id_ex.h"
#include "common_test.h"

// ======================== Instruction Encoding ========================

static inline uint32_t encode_r(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt, uint8_t funct) {
    return ((uint32_t)(rs & 0x1Fu) << 21) |
           ((uint32_t)(rt & 0x1Fu) << 16) |
           ((uint32_t)(rd & 0x1Fu) << 11) |
           ((uint32_t)(shamt & 0x1Fu) << 6) |
           ((uint32_t)(funct & 0x3Fu));
}

static inline uint32_t encode_i(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm) {
    return ((uint32_t)(opcode & 0x3Fu) << 26) |
           ((uint32_t)(rs & 0x1Fu) << 21) |
           ((uint32_t)(rt & 0x1Fu) << 16) |
           ((uint16_t)imm);
}

// ======================== Decode Signal Unpacking ========================

typedef struct cs_expect_t {
    bit reg_dst, alu_src, mem_to_reg, reg_write, mem_read, mem_write, branch, jump;
    bit ops0, ops1, ops2;
} cs_expect_t;

static inline cs_expect_t unpack_cs(const Id_ex_regs *idex) {
    cs_expect_t e;
    word w = {0};
    read_reg32(&idex->decode_signals, w);
    e.reg_dst    = BITN(w, 31);
    e.alu_src    = BITN(w, 30);
    e.mem_to_reg = BITN(w, 29);
    e.reg_write  = BITN(w, 28);
    e.mem_read   = BITN(w, 27);
    e.mem_write  = BITN(w, 26);
    e.branch     = BITN(w, 25);
    e.jump       = BITN(w, 24);
    e.ops0       = BITN(w, 23);
    e.ops1       = BITN(w, 22);
    e.ops2       = BITN(w, 21);
    return e;
}

static inline int assert_cs_eq(const char *prefix, const cs_expect_t *got, const cs_expect_t *exp) {
    char name[256];
#define CHECK(field) do { \
        snprintf(name, sizeof(name), "%s.%s", prefix, #field); \
        if (((got->field)&1) != ((exp->field)&1)) { \
            printf("[FAIL] %s: got=%d expected=%d\n", name, (int)((got->field)&1), (int)((exp->field)&1)); \
            return 1; \
        } else { \
            printf("[PASS] %s\n", name); \
        } \
    } while (0)
    CHECK(reg_dst); CHECK(alu_src); CHECK(mem_to_reg); CHECK(reg_write);
    CHECK(mem_read); CHECK(mem_write); CHECK(branch); CHECK(jump);
    CHECK(ops0); CHECK(ops1); CHECK(ops2);
#undef CHECK
    return 0;
}

// ======================== Helpers ========================

static inline void idex_tick(Id_ex_regs *idex, const If_id_regs *ifid,
                             const Reg324file_ *rf, bit write, bit flush) {
    Id_ex_write w = {write, flush};
    id_ex_regs_step(idex, ifid, rf, &w, 0);
    id_ex_regs_step(idex, ifid, rf, &w, 1);
}

static inline void ifid_load(If_id_regs *ifid, uint32_t instr_u32, uint32_t pc_plus4_u32) {
    reg32_write_u32(&ifid->instr, instr_u32);
    reg32_write_u32(&ifid->pc_plus4, pc_plus4_u32);
}

static inline void rf_load(Reg324file_ *rf, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    reg32_write_u32(&rf->r0, r0);
    reg32_write_u32(&rf->r1, r1);
    reg32_write_u32(&rf->r2, r2);
    reg32_write_u32(&rf->r3, r3);
}

// ======================== Tests ========================

static int test_idex_imm_signext(void) {
    printf("\n=== test_idex_imm_signext ===\n");
    If_id_regs ifid; Reg324file_ rf; Id_ex_regs idex;
    init_if_id_regs(&ifid); init_reg32file(&rf); init_id_ex_regs(&idex);
    rf_load(&rf, 0x11111111, 0x22222222, 0x33333333, 0x44444444);

    // ADDI imm=+1
    ifid_load(&ifid, encode_i(OP_ADDI, 2, 1, (int16_t)0x0001), 0x10);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    ASSERT_EQ_U32("imm_ext for +1", reg32_read_u32(&idex.imm_ext), 0x00000001);

    // ADDI imm=-1
    ifid_load(&ifid, encode_i(OP_ADDI, 2, 1, (int16_t)0xFFFF), 0x14);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    ASSERT_EQ_U32("imm_ext for -1", reg32_read_u32(&idex.imm_ext), 0xFFFFFFFF);

    return 0;
}

static int test_idex_control_signals_types(void) {
    printf("\n=== test_idex_control_signals_types ===\n");
    If_id_regs ifid; Reg324file_ rf; Id_ex_regs idex;
    init_if_id_regs(&ifid); init_reg32file(&rf); init_id_ex_regs(&idex);
    rf_load(&rf, 0, 0x11111111, 0x22222222, 0x33333333);

    // R-type ADD
    ifid_load(&ifid, encode_r(2, 3, 1, 0, FUNCT_ADD), 0x20);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    cs_expect_t got = unpack_cs(&idex);
    cs_expect_t exp = {1, 0, 0, 1, 0, 0, 0, 0, OPS_ADD_[0], OPS_ADD_[1], OPS_ADD_[2]};
    if (assert_cs_eq("R/ADD", &got, &exp)) return 1;

    // ADDI
    ifid_load(&ifid, encode_i(OP_ADDI, 2, 1, 7), 0x24);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    got = unpack_cs(&idex);
    exp = (cs_expect_t){0, 1, 0, 1, 0, 0, 0, 0, OPS_ADD_[0], OPS_ADD_[1], OPS_ADD_[2]};
    if (assert_cs_eq("I/ADDI", &got, &exp)) return 1;

    // LW
    ifid_load(&ifid, encode_i(OP_LW, 2, 1, 0x10), 0x28);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    got = unpack_cs(&idex);
    exp = (cs_expect_t){0, 1, 1, 1, 1, 0, 0, 0, OPS_ADD_[0], OPS_ADD_[1], OPS_ADD_[2]};
    if (assert_cs_eq("I/LW", &got, &exp)) return 1;

    // SW
    ifid_load(&ifid, encode_i(OP_SW, 2, 1, 0x10), 0x2C);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    got = unpack_cs(&idex);
    exp = (cs_expect_t){0, 1, 0, 0, 0, 1, 0, 0, OPS_ADD_[0], OPS_ADD_[1], OPS_ADD_[2]};
    if (assert_cs_eq("I/SW", &got, &exp)) return 1;

    // BEQ
    ifid_load(&ifid, encode_i(OP_BEQ, 2, 1, 2), 0x30);
    idex_tick(&idex, &ifid, &rf, 1, 0);
    got = unpack_cs(&idex);
    exp = (cs_expect_t){0, 0, 0, 0, 0, 0, 1, 0, OPS_SUB_[0], OPS_SUB_[1], OPS_SUB_[2]};
    if (assert_cs_eq("I/BEQ", &got, &exp)) return 1;

    return 0;
}

static int test_idex_random_reg_select(void) {
    printf("\n=== test_idex_random_reg_select (5000 cases) ===\n");
    If_id_regs ifid; Reg324file_ rf; Id_ex_regs idex;
    init_if_id_regs(&ifid); init_reg32file(&rf); init_id_ex_regs(&idex);

    const uint32_t rv[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    rf_load(&rf, rv[0], rv[1], rv[2], rv[3]);

    for (int i = 0; i < 5000; i++) {
        uint8_t rs = (uint8_t)(rand() & 3);
        uint8_t rt = (uint8_t)(rand() & 3);
        uint8_t rd = (uint8_t)(rand() & 3);

        ifid_load(&ifid, encode_r(rs, rt, rd, 0, FUNCT_ADD), 0x100);
        idex_tick(&idex, &ifid, &rf, 1, 0);

        uint32_t got_rs = reg32_read_u32(&idex.read_data1);
        uint32_t got_rt = reg32_read_u32(&idex.read_data2);
        uint32_t got_rs_idx = reg32_read_u32(&idex.rs_idx) & 0x3u;
        uint32_t got_rt_idx = reg32_read_u32(&idex.rt_idx) & 0x3u;
        uint32_t got_rd_idx = reg32_read_u32(&idex.rd_idx) & 0x3u;

        if (got_rs != rv[rs] || got_rt != rv[rt] ||
            got_rs_idx != rs || got_rt_idx != rt || got_rd_idx != rd) {
            printf("[FAIL] case=%d mismatch\n", i);
            return 1;
        }
    }
    PASS("random reg select 5000 ok");
    return 0;
}

static int test_idex_stall_hold(void) {
    printf("\n=== test_idex_stall_hold ===\n");
    If_id_regs ifid; Reg324file_ rf; Id_ex_regs idex;
    init_if_id_regs(&ifid); init_reg32file(&rf); init_id_ex_regs(&idex);
    rf_load(&rf, 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu);

    // Write known state
    ifid_load(&ifid, encode_r(2, 3, 1, 0, FUNCT_ADD), 0x10);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    uint32_t snap_cs  = reg32_read_u32(&idex.decode_signals);
    uint32_t snap_r1  = reg32_read_u32(&idex.read_data1);
    uint32_t snap_r2  = reg32_read_u32(&idex.read_data2);
    uint32_t snap_imm = reg32_read_u32(&idex.imm_ext);

    // Stall: write=0, different instruction
    ifid_load(&ifid, encode_i(OP_ADDI, 1, 0, 0x1234), 0x14);
    idex_tick(&idex, &ifid, &rf, 0, 0);

    ASSERT_EQ_U32("cs hold",  reg32_read_u32(&idex.decode_signals), snap_cs);
    ASSERT_EQ_U32("r1 hold",  reg32_read_u32(&idex.read_data1), snap_r1);
    ASSERT_EQ_U32("r2 hold",  reg32_read_u32(&idex.read_data2), snap_r2);
    ASSERT_EQ_U32("imm hold", reg32_read_u32(&idex.imm_ext), snap_imm);

    return 0;
}

static int test_idex_flush_bubble(void) {
    printf("\n=== test_idex_flush_bubble ===\n");
    If_id_regs ifid; Reg324file_ rf; Id_ex_regs idex;
    init_if_id_regs(&ifid); init_reg32file(&rf); init_id_ex_regs(&idex);
    rf_load(&rf, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);

    // Build non-zero state
    ifid_load(&ifid, encode_i(OP_LW, 2, 1, 0x10), 0x100);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    // Flush
    ifid_load(&ifid, encode_r(2, 3, 1, 0, FUNCT_ADD), 0x104);
    idex_tick(&idex, &ifid, &rf, 0, 1);

    ASSERT_EQ_U32("decode_signals bubble == 0", reg32_read_u32(&idex.decode_signals), 0u);
    ASSERT_EQ_U32("read_data1 bubble == 0",     reg32_read_u32(&idex.read_data1), 0u);
    ASSERT_EQ_U32("read_data2 bubble == 0",     reg32_read_u32(&idex.read_data2), 0u);
    ASSERT_EQ_U32("imm_ext bubble == 0",        reg32_read_u32(&idex.imm_ext), 0u);
    ASSERT_EQ_U32("rs_idx bubble == 0", reg32_read_u32(&idex.rs_idx) & 3u, 0u);
    ASSERT_EQ_U32("rt_idx bubble == 0", reg32_read_u32(&idex.rt_idx) & 3u, 0u);
    ASSERT_EQ_U32("rd_idx bubble == 0", reg32_read_u32(&idex.rd_idx) & 3u, 0u);
    ASSERT_EQ_U32("pc_plus4 updated even on flush", reg32_read_u32(&idex.pc_plus4), 0x104u);

    return 0;
}

int main(void) {
    srand(0xC0FFEEu);
    int rc = 0;
    RUN_TEST(test_idex_imm_signext);
    RUN_TEST(test_idex_control_signals_types);
    RUN_TEST(test_idex_stall_hold);
    RUN_TEST(test_idex_flush_bubble);
    RUN_TEST(test_idex_random_reg_select);
    TEST_SUMMARY("ID/EX Pipeline Register Tests");
    return rc;
}
