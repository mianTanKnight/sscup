// test_dm.c
// Data Memory tests: basic R/W, gating, byte masks, unaligned policy, 20000 case fuzz

#include <stdio.h>
#include <string.h>
#include "../includes/dm.h"
#include "common_test.h"

#define DM_FUZZ_ITERS 20000

// ======================== Golden Model ========================

static uint32_t model_read_u32(const uint8_t model[DEFAULT_SIZE], uint32_t idx) {
    if ((idx & 3u) != 0) return 0;
    if (idx + 4u > DEFAULT_SIZE) return 0;
    return ((uint32_t)model[idx] << 24) | ((uint32_t)model[idx+1] << 16) |
           ((uint32_t)model[idx+2] << 8) | (uint32_t)model[idx+3];
}

static void model_write_u32(uint8_t model[DEFAULT_SIZE], uint32_t idx, uint32_t data,
                            const bit be[4], bit we, bit clk) {
    if (!(we & clk)) return;
    if ((idx & 3u) != 0) return;
    if (idx + 4u > DEFAULT_SIZE) return;
    uint8_t d[4] = {(data>>24)&0xFF, (data>>16)&0xFF, (data>>8)&0xFF, data&0xFF};
    for (int i = 0; i < 4; i++)
        if (be[i]) model[idx + (uint32_t)i] = d[i];
}

static uint32_t xs32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

// ======================== Tests ========================

static int test_basic_rw_and_async_read(void) {
    printf("\n=== test_basic_rw_and_async_read ===\n");
    Dm_ dm; init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    bit be_all[4] = {1, 1, 1, 1};

    u32_to_word(4, addr);
    u32_to_word(0xDEADBEEF, wdata);

    dm.m_write(&dm, addr, wdata, be_all, 1, 1);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("read back == DEADBEEF", word_to_u32(rdata), 0xDEADBEEF);

    uint8_t before0 = dm.memory[4];
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U8("read is non-destructive", dm.memory[4], before0);

    return 0;
}

static int test_write_gating_we_and_clk(void) {
    printf("\n=== test_write_gating_we_and_clk ===\n");
    Dm_ dm; init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    bit be_all[4] = {1, 1, 1, 1};

    u32_to_word(8, addr);
    u32_to_word(0x12345678, wdata);

    // clk=0 => no write
    dm.m_write(&dm, addr, wdata, be_all, 1, 0);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("clk=0 => unchanged (0)", word_to_u32(rdata), 0);

    // we=0 => no write
    dm.m_write(&dm, addr, wdata, be_all, 0, 1);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("we=0 => no write (0)", word_to_u32(rdata), 0);

    // we=1 clk=1 => write
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("we=1 clk=1 => write happens", word_to_u32(rdata), 0x12345678);

    return 0;
}

static int test_mask_all_16_patterns(void) {
    printf("\n=== test_mask_all_16_patterns ===\n");
    Dm_ dm; init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    u32_to_word(16, addr);

    bit be_all[4] = {1, 1, 1, 1};
    u32_to_word(0x11223344, wdata);
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);

    uint32_t newv = 0xAABBCCDD;

    for (uint32_t m = 0; m < 16; m++) {
        // reset base
        u32_to_word(0x11223344, wdata);
        dm.m_write(&dm, addr, wdata, be_all, 1, 1);

        bit be[4] = {0};
        mask_from_u4(m, be);

        u32_to_word(newv, wdata);
        dm.m_write(&dm, addr, wdata, be, 1, 1);
        dm.m_read(&dm, addr, rdata);
        uint32_t got = word_to_u32(rdata);

        uint8_t base_b[4] = {0x11,0x22,0x33,0x44};
        uint8_t new_b[4]  = {0xAA,0xBB,0xCC,0xDD};
        uint8_t out_b[4];
        for (int i = 0; i < 4; i++) out_b[i] = be[i] ? new_b[i] : base_b[i];
        uint32_t exp = ((uint32_t)out_b[0]<<24)|((uint32_t)out_b[1]<<16)|
                       ((uint32_t)out_b[2]<<8)|(uint32_t)out_b[3];

        char name[64];
        snprintf(name, sizeof(name), "mask=%X", (unsigned)m);
        ASSERT_EQ_U32(name, got, exp);
    }

    return 0;
}

static int test_mask_zero_is_no_write(void) {
    printf("\n=== test_mask_zero_is_no_write ===\n");
    Dm_ dm; init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    u32_to_word(20, addr);
    bit be_all[4] = {1,1,1,1};
    bit be_none[4] = {0,0,0,0};

    u32_to_word(0xCAFEBABE, wdata);
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);

    u32_to_word(0, wdata);
    dm.m_write(&dm, addr, wdata, be_none, 1, 1);

    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("mask=0000 => unchanged", word_to_u32(rdata), 0xCAFEBABE);

    return 0;
}

static int test_unaligned_policy(void) {
    printf("\n=== test_unaligned_policy ===\n");
    Dm_ dm; init_dm_(&dm);

    for (int i = 0; i < 6; i++) dm.memory[i] = (uint8_t)(i * 0x11);

    word addr = {0}, rdata = {0}, wdata = {0};
    bit be_all[4] = {1,1,1,1};

    // read unaligned
    u32_to_word(1, addr);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("forbid: read@1 == 0", word_to_u32(rdata), 0);

    // write unaligned
    u32_to_word(1, addr);
    u32_to_word(0xAABBCCDD, wdata);
    uint8_t b1 = dm.memory[1], b2 = dm.memory[2], b3 = dm.memory[3], b4 = dm.memory[4];
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);

    ASSERT_EQ_U8("forbid: mem[1] unchanged", dm.memory[1], b1);
    ASSERT_EQ_U8("forbid: mem[2] unchanged", dm.memory[2], b2);
    ASSERT_EQ_U8("forbid: mem[3] unchanged", dm.memory[3], b3);
    ASSERT_EQ_U8("forbid: mem[4] unchanged", dm.memory[4], b4);

    return 0;
}

static int test_fuzz_against_golden_model(void) {
    printf("\n=== test_fuzz_against_golden_model (%d iters) ===\n", DM_FUZZ_ITERS);
    Dm_ dm; init_dm_(&dm);
    uint8_t model[DEFAULT_SIZE];
    memset(model, 0, sizeof(model));

    uint32_t seed = 0xC0FFEE01u;
    word addr = {0}, wdata = {0}, rdata = {0};

    for (int it = 0; it < DM_FUZZ_ITERS; it++) {
        uint32_t r = xs32(&seed);
        uint32_t idx;
        switch (r & 7u) {
            case 0: idx = DEFAULT_SIZE + (xs32(&seed) % 32); break;
            case 1: idx = (DEFAULT_SIZE - 1) - (xs32(&seed) % 8); break;
            default: idx = xs32(&seed) % (DEFAULT_SIZE + 16); break;
        }

        uint32_t data = xs32(&seed);
        bit be[4];
        mask_from_u4(xs32(&seed) & 0xFu, be);
        bit we  = (xs32(&seed) >> 0) & 1u;
        bit clk = (xs32(&seed) >> 1) & 1u;

        u32_to_word(idx, addr);
        u32_to_word(data, wdata);
        dm.m_write(&dm, addr, wdata, be, we, clk);
        model_write_u32(model, idx, data, be, we, clk);

        if ((xs32(&seed) & 3u) == 0) {
            dm.m_read(&dm, addr, rdata);
            uint32_t got = word_to_u32(rdata);
            uint32_t exp = model_read_u32(model, idx);
            if (got != exp) {
                printf("[FAIL] fuzz mismatch at it=%d idx=%u\n", it, idx);
                return 1;
            }
        }
    }

    PASS("fuzz matched golden model");
    return 0;
}

int main(void) {
    printf("=== TEST: DM Full Regression ===\n");
    int rc = 0;
    RUN_TEST(test_basic_rw_and_async_read);
    RUN_TEST(test_write_gating_we_and_clk);
    RUN_TEST(test_mask_zero_is_no_write);
    RUN_TEST(test_mask_all_16_patterns);
    RUN_TEST(test_unaligned_policy);
    RUN_TEST(test_fuzz_against_golden_model);
    TEST_SUMMARY("Data Memory Tests");
    return rc;
}
