//
// Created by wenshen on 2026/1/20.
//

// Created by wenshen on 2026/1/20.
// test_mem_wb.c

#include <stdio.h>
#include <stdint.h>

#include "includes/common_test.h"
#include "includes/ex_mem.h"
#include "includes/mem_wb.h"
#include "includes/dm_.h"

// ------------------------------------------------------------
// 本测试假设以下位约定：
//   ex_mem.mem_single.bit30 = mem_write
//   mem_wb.wb_single.bit31  = reg_write
//   mem_wb.wb_single.bit30  = mem_to_reg (data_src_to_reg)
//
// DM 策略：禁止非对齐 (addr & 3 != 0 => read/write return 1, 且不改内存)
// ------------------------------------------------------------

static inline void reg32_write_u32_local(Reg32_ *r, uint32_t v) {
    word din = {0};
    word out = {0};
    word_from_u32(v, din);
    reg32_step(r, 1, din, out, 0);
    reg32_step(r, 1, din, out, 1);
}

static inline uint32_t reg32_read_u32_local(const Reg32_ *r) {
    word w = {0};
    read_reg32((Reg32_*)r, w);
    return u32_from_word_local(w);
}

static inline void exmem_set_u32(Ex_mem_regs *exm,
                                uint32_t mem_single,
                                uint32_t wb_single,
                                uint32_t alu_result,
                                uint32_t write_data,
                                uint32_t write_reg_idx)
{
    reg32_write_u32_local(&exm->mem_single, mem_single);
    reg32_write_u32_local(&exm->wb_single, wb_single);
    reg32_write_u32_local(&exm->alu_result, alu_result);
    reg32_write_u32_local(&exm->write_data, write_data);
    reg32_write_u32_local(&exm->write_reg_idx, write_reg_idx);
}

static inline uint32_t dm_read_u32_local(Dm_ *dm, uint32_t addr_u32, bit *err) {
    word addr = {0}, ret = {0};
    word_from_u32(addr_u32, addr);
    bit e = dm->m_read(dm, addr, ret);
    if (err) *err = e;
    return u32_from_word_local(ret);
}

static inline bit dm_write_u32_local(Dm_ *dm, uint32_t addr_u32, uint32_t data_u32,
                                    const bit mask[4], bit we, bit clk)
{
    word addr = {0}, data = {0};
    word_from_u32(addr_u32, addr);
    word_from_u32(data_u32, data);
    return dm->m_write(dm, addr, data, mask, we, clk);
}

// two-stage 调用一个 MEM/WB step（严格 clk=0 + clk=1）
static inline void memwb_tick(const Ex_mem_regs *exm,
                              Mem_wb_regs *mw,
                              Dm_ *dm,
                              bit writer_enabled[4])
{
    mem_wb_regs_step(exm, mw, dm, writer_enabled, 0);
    mem_wb_regs_step(exm, mw, dm, writer_enabled, 1);
}

// ------------------------------------------------------------
// Test 1: penetrate 锁存 + read 异步值锁存
// ------------------------------------------------------------
static int test_penetrate_and_latch(void) {
    printf("\n=== test_penetrate_and_latch ===\n");

    Ex_mem_regs exm;
    Mem_wb_regs mw;
    Dm_ dm;
    init_ex_eme_regs(&exm);
    init_mem_wb_regs(&mw);
    init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};

    // 预置 DM[0x40] = 0x11223344（用于验证 mem_read_data 锁存）
    dm_write_u32_local(&dm, 0x40, 0x11223344, mask_all, 1, 1);

    // mem_single: mem_write=0（bit30=0）
    // wb_single: reg_write=1(bit31=1), mem_to_reg=1(bit30=1)
    const uint32_t mem_single = 0u;
    const uint32_t wb_single  = (1u<<31) | (1u<<30);

    exmem_set_u32(&exm,
                  mem_single,
                  wb_single,
                  0x00000040,   // alu_result 作为地址
                  0xDEADBEEF,   // write_data（本测试不会写）
                  0x00000002);  // write_reg_idx penetrate

    memwb_tick(&exm, &mw, &dm, mask_all);

    ASSERT_EQ_U32("mw.wb_single latched",
                 reg32_read_u32_local(&mw.wb_single),
                 wb_single);

    ASSERT_EQ_U32("mw.alu_result latched",
                 reg32_read_u32_local(&mw.alu_result),
                 0x00000040);

    ASSERT_EQ_U32("mw.write_reg_idx latched",
                 reg32_read_u32_local(&mw.write_reg_idx),
                 0x00000002);

    ASSERT_EQ_U32("mw.mem_read_data latched from DM",
                 reg32_read_u32_local(&mw.mem_read_data),
                 0x11223344);

    return 0;
}

// ------------------------------------------------------------
// Test 2: store 写时序（clk gating） + mem_write=0 不写
// ------------------------------------------------------------
static int test_store_gating_and_disable(void) {
    printf("\n=== test_store_gating_and_disable ===\n");

    Ex_mem_regs exm;
    Mem_wb_regs mw;
    Dm_ dm;
    init_ex_eme_regs(&exm);
    init_mem_wb_regs(&mw);
    init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    const uint32_t addr = 0x00000020;

    // (A) mem_write=1，但 clk=0 不应写
    // 这里我们直接调用 mem_wb_regs_step 分相测试（不是 memwb_tick）
    exmem_set_u32(&exm,
                  (1u<<30),      // mem_single.bit30=1 mem_write
                  0u,
                  addr,
                  0xAABBCCDD,
                  0);

    // clk=0
    mem_wb_regs_step(&exm, &mw, &dm, mask_all, 0);

    bit err = 0;
    uint32_t v0 = dm_read_u32_local(&dm, addr, &err);
    ASSERT_EQ_BIT("clk=0 DM read aligned err=0", err, 0);
    ASSERT_EQ_U32("clk=0 => store should NOT happen", v0, 0x00000000);

    // clk=1 => 写应生效
    mem_wb_regs_step(&exm, &mw, &dm, mask_all, 1);

    uint32_t v1 = dm_read_u32_local(&dm, addr, &err);
    ASSERT_EQ_U32("clk=1 => store happens", v1, 0xAABBCCDD);

    // (B) mem_write=0 => 不写（即使 clk=1）
    exmem_set_u32(&exm,
                  0u,            // mem_write=0
                  0u,
                  addr,
                  0x11223344,
                  0);

    mem_wb_regs_step(&exm, &mw, &dm, mask_all, 1);
    uint32_t v2 = dm_read_u32_local(&dm, addr, &err);
    ASSERT_EQ_U32("mem_write=0 => store must NOT happen", v2, 0xAABBCCDD);

    return 0;
}

// ------------------------------------------------------------
// Test 3: byte mask 覆盖（抽样 + 16pattern 可选）
// 假设 mask[0] 对应最高字节，mask[3] 对应最低字节（与你 dm_write 的 idx+i 写入一致）
// ------------------------------------------------------------
static uint32_t apply_mask_model(uint32_t oldv, uint32_t newv, uint8_t m4) {
    // m4: b3..b0 分别对应 byte0..byte3 (MSB..LSB)
    uint32_t out = oldv;
    for (int i = 0; i < 4; ++i) {
        if (m4 & (1u << (3 - i))) {
            // i=0 写 MSB byte
            uint32_t shift = (3 - i) * 8;
            uint32_t mask = 0xFFu << shift;
            out = (out & ~mask) | (newv & mask);
        }
    }
    return out;
}

static int test_store_byte_masks(void) {
    printf("\n=== test_store_byte_masks ===\n");

    Ex_mem_regs exm;
    Mem_wb_regs mw;
    Dm_ dm;
    init_ex_eme_regs(&exm);
    init_mem_wb_regs(&mw);
    init_dm_(&dm);

    const uint32_t addr = 0x00000010;
    bit err = 0;

    // init old = 0xFFFFFFFF
    bit mask_all[4] = {1,1,1,1};
    dm_write_u32_local(&dm, addr, 0xFFFFFFFF, mask_all, 1, 1);

    const uint32_t newv = 0xA1B2C3D4;

    // 16 patterns 全测（强）
    for (uint8_t m = 0; m < 16; ++m) {
        // reset old each time
        dm_write_u32_local(&dm, addr, 0xFFFFFFFF, mask_all, 1, 1);

        bit mask[4] = {
            (bit)((m >> 3) & 1), // MSB
            (bit)((m >> 2) & 1),
            (bit)((m >> 1) & 1),
            (bit)((m >> 0) & 1)  // LSB
        };

        exmem_set_u32(&exm,
                      (1u<<30), // mem_write=1
                      0u,
                      addr,
                      newv,
                      0u);

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

// ------------------------------------------------------------
// Test 4: 非对齐策略（禁止）
//   - read/write 返回 err=1
//   - 内存不得被修改
//   - mem_wb 锁存的 read_ret 应为 0（dm_read 会清零 ret）
// ------------------------------------------------------------
static int test_unaligned_forbidden_policy(void) {
    printf("\n=== test_unaligned_forbidden_policy ===\n");

    Ex_mem_regs exm;
    Mem_wb_regs mw;
    Dm_ dm;
    init_ex_eme_regs(&exm);
    init_mem_wb_regs(&mw);
    init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    const uint32_t base = 0x00000020;

    // 先写一个对齐位置，确保有值
    dm_write_u32_local(&dm, base, 0x55667788, mask_all, 1, 1);

    // 尝试对齐+1 写（应失败且不改内存）
    exmem_set_u32(&exm,
                  (1u<<30),  // mem_write=1
                  0u,
                  base + 1,  // unaligned
                  0xAABBCCDD,
                  0u);

    memwb_tick(&exm, &mw, &dm, mask_all);

    bit err = 0;
    uint32_t got = dm_read_u32_local(&dm, base, &err);
    ASSERT_EQ_BIT("aligned read err=0", err, 0);
    ASSERT_EQ_U32("unaligned write should NOT change aligned word", got, 0x55667788);

    // 再验证：unaligned read => err=1 且返回 0
    uint32_t ur = dm_read_u32_local(&dm, base + 1, &err);
    ASSERT_EQ_BIT("unaligned read err=1", err, 1);
    ASSERT_EQ_U32("unaligned read returns 0", ur, 0);

    // mem_wb 锁存 read_ret 也应为 0
    ASSERT_EQ_U32("mw.mem_read_data latched == 0 on unaligned read",
                 reg32_read_u32_local(&mw.mem_read_data),
                 0u);

    return 0;
}

// ------------------------------------------------------------
// Test 5: 末尾边界（最后一个对齐 word 必须可写可读）
//   addr = DEFAULT_SIZE - 4（必须对齐）
// ------------------------------------------------------------
static int test_last_word_boundary(void) {
    printf("\n=== test_last_word_boundary ===\n");

    Ex_mem_regs exm;
    Mem_wb_regs mw;
    Dm_ dm;
    init_ex_eme_regs(&exm);
    init_mem_wb_regs(&mw);
    init_dm_(&dm);

    bit mask_all[4] = {1,1,1,1};
    const uint32_t addr = (uint32_t)(DEFAULT_SIZE - 4);

    // store last word
    exmem_set_u32(&exm,
                  (1u<<30),
                  0u,
                  addr,
                  0xCAFEBABE,
                  0u);

    memwb_tick(&exm, &mw, &dm, mask_all);

    bit err = 0;
    uint32_t got = dm_read_u32_local(&dm, addr, &err);
    ASSERT_EQ_BIT("last aligned read err=0", err, 0);
    ASSERT_EQ_U32("last word read back", got, 0xCAFEBABE);

    // oob read should err=1
    uint32_t oob = dm_read_u32_local(&dm, (uint32_t)DEFAULT_SIZE, &err);
    ASSERT_EQ_BIT("oob read err=1", err, 1);
    ASSERT_EQ_U32("oob read returns 0", oob, 0u);

    return 0;
}

// ------------------------------------------------------------

int main(void) {
    printf("=== TEST: MEM stage + MEM/WB Full Regression ===\n");

    int rc = 0;
    rc |= test_penetrate_and_latch();
    rc |= test_store_gating_and_disable();
    rc |= test_store_byte_masks();
    rc |= test_unaligned_forbidden_policy();
    rc |= test_last_word_boundary();

    if (rc == 0) {
        printf("\nALL MEM/WB TESTS PASSED ✅\n");
    }
    return rc;
}
