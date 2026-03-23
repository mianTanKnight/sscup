// test_wb.c
// WB stage tests: ALU path, MEM path, reg_write disable, idx decode, 5000 case fuzz

#include <stdio.h>
#include <stdint.h>
#include "../includes/wb.h"
#include "../includes/mem_wb.h"
#include "common_test.h"

// ======================== Helpers ========================

static inline void mw_set_u32(Mem_wb_regs *mw,
                             uint32_t wb_single, uint32_t mem_read_data,
                             uint32_t alu_result, uint32_t write_reg_idx) {
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

static inline uint32_t mk_wb_single(bit reg_write, bit mem_to_reg) {
    return ((uint32_t)(reg_write & 1) << 31) | ((uint32_t)(mem_to_reg & 1) << 30);
}

static inline uint32_t mk_idx_u32(uint32_t idx2) {
    return (idx2 & 3u);
}

// ======================== Tests ========================

static int test_wb_alu_path_basic(void) {
    printf("\n=== test_wb_alu_path_basic ===\n");
    Mem_wb_regs mw; Reg324file_ rf;
    init_mem_wb_regs(&mw); init_reg32file(&rf);

    rf_write_idx_force(&rf, 0, 0x00000000);
    rf_write_idx_force(&rf, 1, 0x11111111);
    rf_write_idx_force(&rf, 2, 0x22222222);
    rf_write_idx_force(&rf, 3, 0x33333333);

    mw_set_u32(&mw, mk_wb_single(1, 0), 0xDEADBEEF, 0xABCDEF01, mk_idx_u32(2));

    wb_step(&mw, &rf, 0);
    ASSERT_EQ_U32("clk=0 => R2 unchanged", rf_read_idx(&rf, 2), 0x22222222);

    wb_step(&mw, &rf, 1);
    ASSERT_EQ_U32("clk=1 => R2 from ALU", rf_read_idx(&rf, 2), 0xABCDEF01);
    ASSERT_EQ_U32("R0 unchanged", rf_read_idx(&rf, 0), 0x00000000);
    ASSERT_EQ_U32("R1 unchanged", rf_read_idx(&rf, 1), 0x11111111);
    ASSERT_EQ_U32("R3 unchanged", rf_read_idx(&rf, 3), 0x33333333);

    return 0;
}

static int test_wb_mem_path_basic(void) {
    printf("\n=== test_wb_mem_path_basic ===\n");
    Mem_wb_regs mw; Reg324file_ rf;
    init_mem_wb_regs(&mw); init_reg32file(&rf);

    rf_write_idx_force(&rf, 0, 0xAAAAAAAA);
    rf_write_idx_force(&rf, 1, 0xBBBBBBBB);
    rf_write_idx_force(&rf, 2, 0xCCCCCCCC);
    rf_write_idx_force(&rf, 3, 0xDDDDDDDD);

    mw_set_u32(&mw, mk_wb_single(1, 1), 0x13572468, 0xFFFFFFFF, mk_idx_u32(1));

    wb_step(&mw, &rf, 0);
    ASSERT_EQ_U32("clk=0 => R1 unchanged", rf_read_idx(&rf, 1), 0xBBBBBBBB);

    wb_step(&mw, &rf, 1);
    ASSERT_EQ_U32("clk=1 => R1 from MEM", rf_read_idx(&rf, 1), 0x13572468);
    ASSERT_EQ_U32("R0 unchanged", rf_read_idx(&rf, 0), 0xAAAAAAAA);
    ASSERT_EQ_U32("R2 unchanged", rf_read_idx(&rf, 2), 0xCCCCCCCC);
    ASSERT_EQ_U32("R3 unchanged", rf_read_idx(&rf, 3), 0xDDDDDDDD);

    return 0;
}

static int test_wb_regwrite_disable(void) {
    printf("\n=== test_wb_regwrite_disable ===\n");
    Mem_wb_regs mw; Reg324file_ rf;
    init_mem_wb_regs(&mw); init_reg32file(&rf);

    rf_write_idx_force(&rf, 0, 0x10);
    rf_write_idx_force(&rf, 1, 0x11);
    rf_write_idx_force(&rf, 2, 0x12);
    rf_write_idx_force(&rf, 3, 0x13);

    mw_set_u32(&mw, mk_wb_single(0, 1), 0x11111111, 0x22222222, mk_idx_u32(3));
    wb_step(&mw, &rf, 1);

    ASSERT_EQ_U32("R0 unchanged", rf_read_idx(&rf, 0), 0x10);
    ASSERT_EQ_U32("R1 unchanged", rf_read_idx(&rf, 1), 0x11);
    ASSERT_EQ_U32("R2 unchanged", rf_read_idx(&rf, 2), 0x12);
    ASSERT_EQ_U32("R3 unchanged", rf_read_idx(&rf, 3), 0x13);

    return 0;
}

static int test_wb_idx_decode_all(void) {
    printf("\n=== test_wb_idx_decode_all ===\n");
    Mem_wb_regs mw; Reg324file_ rf;
    init_mem_wb_regs(&mw); init_reg32file(&rf);

    for (int idx = 0; idx < 4; ++idx) {
        rf_write_idx_force(&rf, 0, 0x100);
        rf_write_idx_force(&rf, 1, 0x200);
        rf_write_idx_force(&rf, 2, 0x300);
        rf_write_idx_force(&rf, 3, 0x400);

        uint32_t wv = 0xABC00000u | (uint32_t)idx;
        mw_set_u32(&mw, mk_wb_single(1, 0), 0, wv, mk_idx_u32((uint32_t)idx));
        wb_step(&mw, &rf, 0);
        wb_step(&mw, &rf, 1);

        for (int j = 0; j < 4; ++j) {
            char msg[64];
            snprintf(msg, sizeof(msg), "idx=%d => R%d", idx, j);
            uint32_t expect = (j == idx) ? wv : (j==0?0x100u:(j==1?0x200u:(j==2?0x300u:0x400u)));
            ASSERT_EQ_U32(msg, rf_read_idx(&rf, j), expect);
        }
    }

    return 0;
}

static int test_wb_fuzz_golden(void) {
    printf("\n=== test_wb_fuzz_golden (5000 cases) ===\n");
    Mem_wb_regs mw; Reg324file_ rf;
    init_mem_wb_regs(&mw); init_reg32file(&rf);

    uint32_t g[4] = {0,0,0,0};
    uint32_t seed = 0xC0FFEEu;

    for (int i = 0; i < 5000; ++i) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t alu = seed;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t mem = seed;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        uint32_t idx = seed & 3u;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        bit reg_write = (bit)(seed & 1u);
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        bit mem_to_reg = (bit)(seed & 1u);
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        bit clk = (bit)(seed & 1u);

        mw_set_u32(&mw, mk_wb_single(reg_write, mem_to_reg), mem, alu, mk_idx_u32(idx));
        wb_step(&mw, &rf, 0);
        wb_step(&mw, &rf, clk);

        if (clk == 1 && reg_write == 1)
            g[idx] = mem_to_reg ? mem : alu;

        for (int r = 0; r < 4; ++r) {
            if (rf_read_idx(&rf, r) != g[r]) {
                printf("[FAIL] fuzz mismatch i=%d r=%d\n", i, r);
                return 1;
            }
        }
    }

    PASS("fuzz matched golden model");
    return 0;
}

int main(void) {
    printf("=== TEST: WB Full Regression ===\n");
    int rc = 0;
    RUN_TEST(test_wb_alu_path_basic);
    RUN_TEST(test_wb_mem_path_basic);
    RUN_TEST(test_wb_regwrite_disable);
    RUN_TEST(test_wb_idx_decode_all);
    RUN_TEST(test_wb_fuzz_golden);
    TEST_SUMMARY("WB Stage Tests");
    return rc;
}
