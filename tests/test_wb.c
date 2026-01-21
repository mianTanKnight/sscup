//
// Created by wenshen on 2026/1/21.
//
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../includes/common_test.h"
#include "../includes/wb_.h"
#include "../includes/mem_wb.h"

// -------------------------
// 辅助：写 Mem_wb_regs（two-stage 写寄存器）
// -------------------------
static inline void mw_set_u32(Mem_wb_regs *mw,
                             uint32_t wb_single,
                             uint32_t mem_read_data,
                             uint32_t alu_result,
                             uint32_t write_reg_idx)
{
    reg32_write_u32(&mw->wb_single, wb_single);
    reg32_write_u32(&mw->mem_read_data, mem_read_data);
    reg32_write_u32(&mw->alu_result, alu_result);
    reg32_write_u32(&mw->write_reg_idx, write_reg_idx);
}

static inline uint32_t rf_read_idx(Reg324file_ *rf, int idx) {
    switch (idx & 3) {
        case 0: return reg32_read_u32(&rf->r0);
        case 1: return reg32_read_u32(&rf->r1);
        case 2: return reg32_read_u32(&rf->r2);
        default:return reg32_read_u32(&rf->r3);
    }
}

static inline void rf_write_idx_force(Reg324file_ *rf, int idx, uint32_t v) {
    switch (idx & 3) {
        case 0: reg32_write_u32(&rf->r0, v); break;
        case 1: reg32_write_u32(&rf->r1, v); break;
        case 2: reg32_write_u32(&rf->r2, v); break;
        default:reg32_write_u32(&rf->r3, v); break;
    }
}

// wb_single 位约定：bit31=reg_write, bit30=mem_to_reg
static inline uint32_t mk_wb_single(bit reg_write, bit mem_to_reg) {
    return ((uint32_t)(reg_write & 1) << 31) | ((uint32_t)(mem_to_reg & 1) << 30);
}

// write_reg_idx 约定：只用低2位（bit1..bit0），其余位无所谓
static inline uint32_t mk_idx_u32(uint32_t idx2) {
    return (idx2 & 3u);
}

// -------------------------
// Test 1: ALU path writeback (mem_to_reg=0)
// -------------------------
static int test_wb_alu_path_basic(void) {
    printf("\n=== test_wb_alu_path_basic ===\n");

    Mem_wb_regs mw;
    Reg324file_ rf;
    init_mem_wb_regs(&mw);
    init_reg32file(&rf);

    // init rf with known values
    rf_write_idx_force(&rf, 0, 0x00000000);
    rf_write_idx_force(&rf, 1, 0x11111111);
    rf_write_idx_force(&rf, 2, 0x22222222);
    rf_write_idx_force(&rf, 3, 0x33333333);

    // want write to R2 with alu_result
    const uint32_t wb_single = mk_wb_single(1, 0); // reg_write=1, mem_to_reg=0
    mw_set_u32(&mw, wb_single, 0xDEADBEEF, 0xABCDEF01, mk_idx_u32(2));

    // clk=0: should NOT commit
    wb_step(&mw, &rf, 0);
    ASSERT_EQ_U32("clk=0 => R2 unchanged", rf_read_idx(&rf, 2), 0x22222222);

    // clk=1: commit
    wb_step(&mw, &rf, 1);
    ASSERT_EQ_U32("clk=1 => R2 updated from ALU", rf_read_idx(&rf, 2), 0xABCDEF01);

    // others unchanged
    ASSERT_EQ_U32("R0 unchanged", rf_read_idx(&rf, 0), 0x00000000);
    ASSERT_EQ_U32("R1 unchanged", rf_read_idx(&rf, 1), 0x11111111);
    ASSERT_EQ_U32("R3 unchanged", rf_read_idx(&rf, 3), 0x33333333);

    return 0;
}

// -------------------------
// Test 2: MEM path writeback (mem_to_reg=1)
// -------------------------
static int test_wb_mem_path_basic(void) {
    printf("\n=== test_wb_mem_path_basic ===\n");

    Mem_wb_regs mw;
    Reg324file_ rf;
    init_mem_wb_regs(&mw);
    init_reg32file(&rf);

    rf_write_idx_force(&rf, 0, 0xAAAAAAAA);
    rf_write_idx_force(&rf, 1, 0xBBBBBBBB);
    rf_write_idx_force(&rf, 2, 0xCCCCCCCC);
    rf_write_idx_force(&rf, 3, 0xDDDDDDDD);

    // write to R1 with mem_read_data
    const uint32_t wb_single = mk_wb_single(1, 1); // mem_to_reg=1
    mw_set_u32(&mw, wb_single, 0x13572468, 0xFFFFFFFF, mk_idx_u32(1));

    wb_step(&mw, &rf, 0);
    ASSERT_EQ_U32("clk=0 => R1 unchanged", rf_read_idx(&rf, 1), 0xBBBBBBBB);

    wb_step(&mw, &rf, 1);
    ASSERT_EQ_U32("clk=1 => R1 updated from MEM", rf_read_idx(&rf, 1), 0x13572468);

    // others unchanged
    ASSERT_EQ_U32("R0 unchanged", rf_read_idx(&rf, 0), 0xAAAAAAAA);
    ASSERT_EQ_U32("R2 unchanged", rf_read_idx(&rf, 2), 0xCCCCCCCC);
    ASSERT_EQ_U32("R3 unchanged", rf_read_idx(&rf, 3), 0xDDDDDDDD);

    return 0;
}

// -------------------------
// Test 3: reg_write=0 => no side effects (even clk=1)
// -------------------------
static int test_wb_regwrite_disable(void) {
    printf("\n=== test_wb_regwrite_disable ===\n");

    Mem_wb_regs mw;
    Reg324file_ rf;
    init_mem_wb_regs(&mw);
    init_reg32file(&rf);

    rf_write_idx_force(&rf, 0, 0x00000010);
    rf_write_idx_force(&rf, 1, 0x00000011);
    rf_write_idx_force(&rf, 2, 0x00000012);
    rf_write_idx_force(&rf, 3, 0x00000013);

    const uint32_t wb_single = mk_wb_single(0, 1); // reg_write=0
    mw_set_u32(&mw, wb_single, 0x11111111, 0x22222222, mk_idx_u32(3));

    wb_step(&mw, &rf, 1);

    ASSERT_EQ_U32("R0 unchanged", rf_read_idx(&rf, 0), 0x00000010);
    ASSERT_EQ_U32("R1 unchanged", rf_read_idx(&rf, 1), 0x00000011);
    ASSERT_EQ_U32("R2 unchanged", rf_read_idx(&rf, 2), 0x00000012);
    ASSERT_EQ_U32("R3 unchanged", rf_read_idx(&rf, 3), 0x00000013);

    return 0;
}

// -------------------------
// Test 4: idx decode covers 0..3 (one-hot correctness)
// -------------------------
static int test_wb_idx_decode_all(void) {
    printf("\n=== test_wb_idx_decode_all ===\n");

    Mem_wb_regs mw;
    Reg324file_ rf;
    init_mem_wb_regs(&mw);
    init_reg32file(&rf);

    // init all regs = different
    rf_write_idx_force(&rf, 0, 0x100);
    rf_write_idx_force(&rf, 1, 0x200);
    rf_write_idx_force(&rf, 2, 0x300);
    rf_write_idx_force(&rf, 3, 0x400);

    const uint32_t wb_single = mk_wb_single(1, 0);

    for (int idx = 0; idx < 4; ++idx) {
        // reset each round
        rf_write_idx_force(&rf, 0, 0x100);
        rf_write_idx_force(&rf, 1, 0x200);
        rf_write_idx_force(&rf, 2, 0x300);
        rf_write_idx_force(&rf, 3, 0x400);

        const uint32_t wv = 0xABC00000u | (uint32_t)idx;
        mw_set_u32(&mw, wb_single, 0, wv, mk_idx_u32((uint32_t)idx));

        wb_step(&mw, &rf, 0);
        wb_step(&mw, &rf, 1);

        for (int j = 0; j < 4; ++j) {
            char msg[64];
            snprintf(msg, sizeof(msg), "idx=%d => R%d check", idx, j);
            uint32_t expect = (j == idx) ? wv : (j==0?0x100u:(j==1?0x200u:(j==2?0x300u:0x400u)));
            ASSERT_EQ_U32(msg, rf_read_idx(&rf, j), expect);
        }
    }

    return 0;
}

// -------------------------
// Test 5: fuzz vs golden model (5000 cases)
// -------------------------
static uint32_t pick_wdata(uint32_t alu, uint32_t mem, bit mem_to_reg) {
    return mem_to_reg ? mem : alu;
}

static int test_wb_fuzz_golden(int N) {
    printf("\n=== test_wb_fuzz_golden (%d cases) ===\n", N);

    Mem_wb_regs mw;
    Reg324file_ rf;
    init_mem_wb_regs(&mw);
    init_reg32file(&rf);

    // golden shadow
    uint32_t g[4] = {0,0,0,0};

    // seed deterministic
    uint32_t seed = 0xC0FFEEu;

    for (int i = 0; i < N; ++i) {
        // xorshift32
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;

        uint32_t alu = seed;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t mem = seed;

        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t idx = seed & 3u;

        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        bit reg_write = (bit)(seed & 1u);

        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        bit mem_to_reg = (bit)(seed & 1u);

        // Random choose clk (0 or 1)
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        bit clk = (bit)(seed & 1u);

        uint32_t wb_single = mk_wb_single(reg_write, mem_to_reg);
        mw_set_u32(&mw, wb_single, mem, alu, mk_idx_u32(idx));

        // apply DUT
        wb_step(&mw, &rf, 0);
        wb_step(&mw, &rf, clk);

        // apply golden
        if (clk == 1 && reg_write == 1) {
            g[idx] = pick_wdata(alu, mem, mem_to_reg);
        }

        // compare
        for (int r = 0; r < 4; ++r) {
            uint32_t got = rf_read_idx(&rf, r);
            if (got != g[r]) {
                printf("[FAIL] fuzz mismatch i=%d r=%d got=0x%08X exp=0x%08X "
                       "(idx=%u reg_write=%d mem_to_reg=%d clk=%d alu=0x%08X mem=0x%08X)\n",
                       i, r, got, g[r], idx, reg_write, mem_to_reg, clk, alu, mem);
                return 1;
            }
        }
    }

    printf("[PASS] fuzz matched golden model\n");
    return 0;
}

// int main(void) {
//     printf("=== TEST: WB Full Regression ===\n");
//
//     int rc = 0;
//     rc |= test_wb_alu_path_basic();
//     rc |= test_wb_mem_path_basic();
//     rc |= test_wb_regwrite_disable();
//     rc |= test_wb_idx_decode_all();
//     rc |= test_wb_fuzz_golden(5000);
//
//     if (rc == 0) {
//         printf("\nALL WB TESTS PASSED ✅\n");
//     }
//     return rc;
// }
