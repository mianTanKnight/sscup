// test_mem_wb.c
// MEM/WB pipeline register tests: latch, store gating, byte mask, unaligned, boundary

#include <stdio.h>
#include "../includes/mem_wb.h"
#include "../includes/dm.h"
#include "common_test.h"

// ======================== Helpers ========================

static inline void exmem_set_u32(Ex_mem_regs *exm,
                                uint32_t mem_single, uint32_t wb_single,
                                uint32_t alu_result, uint32_t write_data,
                                uint32_t write_reg_idx) {
    reg32_write_u32(&exm->mem_single, mem_single);
    reg32_write_u32(&exm->wb_single, wb_single);
    reg32_write_u32(&exm->alu_result, alu_result);
    reg32_write_u32(&exm->write_data, write_data);
    reg32_write_u32(&exm->write_reg_idx, write_reg_idx);
}

static inline uint32_t dm_read_u32_local(Dm_ *dm, uint32_t addr_u32, bit *err) {
    word addr = {0}, ret = {0};
    word_from_u32(addr_u32, addr);
    bit e = dm->m_read(dm, addr, ret);
    if (err) *err = e;
    return word_to_u32(ret);
}

static inline void memwb_tick(const Ex_mem_regs *exm, Mem_wb_regs *mw,
                              Dm_ *dm, bit writer_enabled[4]) {
    mem_wb_regs_step(exm, mw, dm, writer_enabled, 0);
    mem_wb_regs_step(exm, mw, dm, writer_enabled, 1);
}

// ======================== Tests ========================

static int test_penetrate_and_latch(void) {
    printf("\n=== test_penetrate_and_latch ===\n");

    Ex_mem_regs exm; Mem_wb_regs mw; Dm_ dm;
    init_ex_mem_regs(&exm); init_mem_wb_regs(&mw); init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    dm_write_u32_local(&dm, 0x40, 0x11223344, mask_all, 1, 1);

    const uint32_t wb_single = (1u<<31) | (1u<<30);
    exmem_set_u32(&exm, 0u, wb_single, 0x00000040, 0xDEADBEEF, 0x00000002);

    memwb_tick(&exm, &mw, &dm, mask_all);

    ASSERT_EQ_U32("mw.wb_single latched",       reg32_read_u32(&mw.wb_single), wb_single);
    ASSERT_EQ_U32("mw.alu_result latched",       reg32_read_u32(&mw.alu_result), 0x00000040);
    ASSERT_EQ_U32("mw.write_reg_idx latched",    reg32_read_u32(&mw.write_reg_idx), 0x00000002);
    ASSERT_EQ_U32("mw.mem_read_data from DM",    reg32_read_u32(&mw.mem_read_data), 0x11223344);

    return 0;
}

static int test_store_gating_and_disable(void) {
    printf("\n=== test_store_gating_and_disable ===\n");

    Ex_mem_regs exm; Mem_wb_regs mw; Dm_ dm;
    init_ex_mem_regs(&exm); init_mem_wb_regs(&mw); init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    const uint32_t addr = 0x00000020;

    // mem_write=1, clk=0 => should NOT write
    exmem_set_u32(&exm, (1u<<30), 0u, addr, 0xAABBCCDD, 0);
    mem_wb_regs_step(&exm, &mw, &dm, mask_all, 0);

    bit err = 0;
    ASSERT_EQ_U32("clk=0 => store should NOT happen", dm_read_u32_local(&dm, addr, &err), 0x00000000);

    // clk=1 => write happens
    mem_wb_regs_step(&exm, &mw, &dm, mask_all, 1);
    ASSERT_EQ_U32("clk=1 => store happens", dm_read_u32_local(&dm, addr, &err), 0xAABBCCDD);

    // mem_write=0 => no write even clk=1
    exmem_set_u32(&exm, 0u, 0u, addr, 0x11223344, 0);
    mem_wb_regs_step(&exm, &mw, &dm, mask_all, 1);
    ASSERT_EQ_U32("mem_write=0 => store must NOT happen", dm_read_u32_local(&dm, addr, &err), 0xAABBCCDD);

    return 0;
}

static uint32_t apply_mask_model(uint32_t oldv, uint32_t newv, uint8_t m4) {
    uint32_t out = oldv;
    for (int i = 0; i < 4; ++i) {
        if (m4 & (1u << (3 - i))) {
            uint32_t shift = (3 - i) * 8;
            uint32_t mask = 0xFFu << shift;
            out = (out & ~mask) | (newv & mask);
        }
    }
    return out;
}

static int test_store_byte_masks(void) {
    printf("\n=== test_store_byte_masks ===\n");

    Ex_mem_regs exm; Mem_wb_regs mw; Dm_ dm;
    init_ex_mem_regs(&exm); init_mem_wb_regs(&mw); init_dm_(&dm);

    const uint32_t addr = 0x00000010;
    bit mask_all[4] = {1,1,1,1};
    bit err = 0;
    const uint32_t newv = 0xA1B2C3D4;

    for (uint8_t m = 0; m < 16; ++m) {
        dm_write_u32_local(&dm, addr, 0xFFFFFFFF, mask_all, 1, 1);

        bit mask[4] = {
            (bit)((m >> 3) & 1),
            (bit)((m >> 2) & 1),
            (bit)((m >> 1) & 1),
            (bit)((m >> 0) & 1)
        };

        exmem_set_u32(&exm, (1u<<30), 0u, addr, newv, 0u);
        mem_wb_regs_step(&exm, &mw, &dm, mask, 0);
        mem_wb_regs_step(&exm, &mw, &dm, mask, 1);

        uint32_t got = dm_read_u32_local(&dm, addr, &err);
        uint32_t exp = apply_mask_model(0xFFFFFFFFu, newv, m);

        char msg[64];
        snprintf(msg, sizeof(msg), "mask=0x%X => expected word", m);
        ASSERT_EQ_U32(msg, got, exp);
    }

    return 0;
}

static int test_unaligned_forbidden_policy(void) {
    printf("\n=== test_unaligned_forbidden_policy ===\n");

    Ex_mem_regs exm; Mem_wb_regs mw; Dm_ dm;
    init_ex_mem_regs(&exm); init_mem_wb_regs(&mw); init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    const uint32_t base = 0x00000020;

    dm_write_u32_local(&dm, base, 0x55667788, mask_all, 1, 1);

    // Unaligned write should fail
    exmem_set_u32(&exm, (1u<<30), 0u, base + 1, 0xAABBCCDD, 0u);
    memwb_tick(&exm, &mw, &dm, mask_all);

    bit err = 0;
    ASSERT_EQ_U32("unaligned write should NOT change aligned word",
                  dm_read_u32_local(&dm, base, &err), 0x55667788);

    // Unaligned read => err=1 and returns 0
    uint32_t ur = dm_read_u32_local(&dm, base + 1, &err);
    ASSERT_EQ_BIT("unaligned read err=1", err, 1);
    ASSERT_EQ_U32("unaligned read returns 0", ur, 0);

    return 0;
}

static int test_last_word_boundary(void) {
    printf("\n=== test_last_word_boundary ===\n");

    Ex_mem_regs exm; Mem_wb_regs mw; Dm_ dm;
    init_ex_mem_regs(&exm); init_mem_wb_regs(&mw); init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    const uint32_t addr = (uint32_t)(DEFAULT_SIZE - 4);

    exmem_set_u32(&exm, (1u<<30), 0u, addr, 0xCAFEBABE, 0u);
    memwb_tick(&exm, &mw, &dm, mask_all);

    bit err = 0;
    ASSERT_EQ_U32("last word read back", dm_read_u32_local(&dm, addr, &err), 0xCAFEBABE);

    uint32_t oob = dm_read_u32_local(&dm, (uint32_t)DEFAULT_SIZE, &err);
    ASSERT_EQ_BIT("oob read err=1", err, 1);
    ASSERT_EQ_U32("oob read returns 0", oob, 0u);

    return 0;
}

int main(void) {
    printf("=== TEST: MEM/WB Full Regression ===\n");
    int rc = 0;
    RUN_TEST(test_penetrate_and_latch);
    RUN_TEST(test_store_gating_and_disable);
    RUN_TEST(test_store_byte_masks);
    RUN_TEST(test_unaligned_forbidden_policy);
    RUN_TEST(test_last_word_boundary);
    TEST_SUMMARY("MEM/WB Pipeline Register Tests");
    return rc;
}
