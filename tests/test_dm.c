//
// test_dm_full.c - Comprehensive DM regression tests
// Created by wenshen + ChatGPT on 2026/01/20
//
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../includes/common.h"
#include "../includes/dm.h"
#include "../includes/utils.h"

// ====================== POLICY SWITCHES ======================
// 1) Unaligned address policy:
//    1 = ALLOW unaligned (byte-addressable, current behavior)
//    0 = FORBID unaligned (idx%4!=0 => read=0, write=ignore)
#define DM_ALLOW_UNALIGNED 0

// 2) Near-end policy (idx close to DEFAULT_SIZE):
//    1 = TRUNCATE: read returns remaining bytes + zeros; write writes only remaining bytes
//    0 = STRICT_OOB: if idx+4 > DEFAULT_SIZE => treat as OOB (read=0, write=ignore)
#define DM_TRUNCATE_NEAR_END 1

// 3) Random stress test iterations:
#define DM_FUZZ_ITERS 20000
// =============================================================

// --------------------- Assert macros ---------------------
#define FAILF(...) do { printf("[FAIL] "); printf(__VA_ARGS__); printf("\n"); return 1; } while(0)
#define PASS(msg)  do { printf("[PASS] %s\n", (msg)); } while(0)

#define ASSERT_EQ_U32(name, got, exp) do { \
    uint32_t g__ = (uint32_t)(got); \
    uint32_t e__ = (uint32_t)(exp); \
    if (g__ != e__) { \
        printf("[FAIL] %s: got=0x%08X (%u) expected=0x%08X (%u)\n", \
               (name), (unsigned)g__, (unsigned)g__, (unsigned)e__, (unsigned)e__); \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
    } \
} while(0)

#define ASSERT_EQ_U8(name, got, exp) do { \
    uint8_t g__ = (uint8_t)(got); \
    uint8_t e__ = (uint8_t)(exp); \
    if (g__ != e__) { \
        printf("[FAIL] %s: got=0x%02X expected=0x%02X\n", (name), g__, e__); \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
    } \
} while(0)


// ============================================================
// Golden reference model (byte-addressable memory semantics)
// This is the "spec" your DM should match under policy switches.
// ============================================================
static uint32_t read_u32_be_bytes(const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3) {
    return ((uint32_t) b0 << 24) | ((uint32_t) b1 << 16) | ((uint32_t) b2 << 8) | (uint32_t) b3;
}

static void u32_to_be_bytes(uint32_t v, uint8_t out4[4]) {
    out4[0] = (v >> 24) & 0xFF;
    out4[1] = (v >> 16) & 0xFF;
    out4[2] = (v >> 8) & 0xFF;
    out4[3] = (v >> 0) & 0xFF;
}

static uint32_t model_read_u32(const uint8_t model[DEFAULT_SIZE], uint32_t idx) {
#if !DM_ALLOW_UNALIGNED
    if ((idx & 3u) != 0) return 0;
#endif
#if !DM_TRUNCATE_NEAR_END
    if (idx >= DEFAULT_SIZE) return 0;
    if (idx + 4u > DEFAULT_SIZE) return 0;
    return read_u32_be_bytes(model[idx], model[idx + 1], model[idx + 2], model[idx + 3]);
#else
    if (idx >= DEFAULT_SIZE) return 0;
    uint8_t b[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        uint32_t p = idx + (uint32_t) i;
        if (p < DEFAULT_SIZE) b[i] = model[p];
        else b[i] = 0;
    }
    return read_u32_be_bytes(b[0], b[1], b[2], b[3]);
#endif
}

static void model_write_u32(uint8_t model[DEFAULT_SIZE], uint32_t idx, uint32_t data, const bit be[4], bit we,
                            bit clk) {
    if (!(we & clk)) return;
#if !DM_ALLOW_UNALIGNED
    if ((idx & 3u) != 0) return;
#endif
#if !DM_TRUNCATE_NEAR_END
    if (idx >= DEFAULT_SIZE) return;
    if (idx + 4u > DEFAULT_SIZE) return;
    uint8_t d[4]; u32_to_be_bytes(data, d);
    for (int i = 0; i < 4; i++) if (be[i]) model[idx + (uint32_t) i] = d[i];
#else
    if (idx >= DEFAULT_SIZE) return;
    uint8_t d[4];
    u32_to_be_bytes(data, d);
    for (int i = 0; i < 4; i++) {
        uint32_t p = idx + (uint32_t) i;
        if (p < DEFAULT_SIZE) {
            if (be[i]) model[p] = d[i];
        }
    }
#endif
}

// Simple deterministic RNG (xorshift32)
static uint32_t xs32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}


// ============================================================
// Tests
// ============================================================
static int test_basic_rw_and_async_read(void) {
    printf("\n=== test_basic_rw_and_async_read ===\n");
    Dm_ dm;
    init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    bit be_all[4] = {1, 1, 1, 1};

    u32_to_word(4, addr);
    u32_to_word(0xDEADBEEF, wdata);

    // write at clk=1
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);
    dm.m_read(&dm, addr, rdata);

    ASSERT_EQ_U32("read back == DEADBEEF", word_to_u32(rdata), 0xDEADBEEF);

    // read should NOT modify memory
    uint32_t before0 = dm.memory[4];
    dm.m_read(&dm, addr, rdata);
    uint32_t after0 = dm.memory[4];
    ASSERT_EQ_U8("read is non-destructive (byte0)", after0, before0);

    return 0;
}

static int test_write_gating_we_and_clk(void) {
    printf("\n=== test_write_gating_we_and_clk ===\n");
    Dm_ dm;
    init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    bit be_all[4] = {1, 1, 1, 1};

    u32_to_word(8, addr);
    u32_to_word(0x12345678, wdata);

    // clk=0 => no write
    dm.m_write(&dm, addr, wdata, be_all, 1, 0);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("clk=0 => memory unchanged (0)", word_to_u32(rdata), 0);

    // we=0 => no write even if clk=1
    dm.m_write(&dm, addr, wdata, be_all, 0, 1);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("we=0 => no write (0)", word_to_u32(rdata), 0);

    // we=1 clk=1 => write happens
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);
    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("we=1 clk=1 => write happens", word_to_u32(rdata), 0x12345678);

    return 0;
}

static int test_mask_all_16_patterns(void) {
    printf("\n=== test_mask_all_16_patterns ===\n");
    Dm_ dm;
    init_dm_(&dm);

    // Initialize word at addr with known base
    word addr = {0}, wdata = {0}, rdata = {0};
    u32_to_word(16, addr);

    // base = 0x11223344
    u32_to_word(0x11223344, wdata);
    bit be_all[4] = {1, 1, 1, 1};
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);

    // Try every mask writing new = 0xAABBCCDD
    uint32_t newv = 0xAABBCCDD;

    for (uint32_t m = 0; m < 16; m++) {
        // reset base each round
        dm.m_write(&dm, addr, (word){0}, (bit[4]){0, 0, 0, 0}, 0, 1); // no-op (clarity)
        u32_to_word(0x11223344, wdata);
        dm.m_write(&dm, addr, wdata, be_all, 1, 1);

        bit be[4] = {0, 0, 0, 0};
        mask_from_u4(m, be);

        u32_to_word(newv, wdata);
        dm.m_write(&dm, addr, wdata, be, 1, 1);
        dm.m_read(&dm, addr, rdata);
        uint32_t got = word_to_u32(rdata);

        // compute expected in host
        uint8_t base_b[4], new_b[4], out_b[4];
        u32_to_be_bytes(0x11223344, base_b);
        u32_to_be_bytes(newv, new_b);
        for (int i = 0; i < 4; i++) out_b[i] = be[i] ? new_b[i] : base_b[i];
        uint32_t exp = read_u32_be_bytes(out_b[0], out_b[1], out_b[2], out_b[3]);

        char name[128];
        snprintf(name, sizeof(name), "mask=%X => expected word", (unsigned) m);
        ASSERT_EQ_U32(name, got, exp);
    }

    return 0;
}

static int test_mask_zero_is_no_write(void) {
    printf("\n=== test_mask_zero_is_no_write ===\n");
    Dm_ dm;
    init_dm_(&dm);

    word addr = {0}, wdata = {0}, rdata = {0};
    u32_to_word(20, addr);

    bit be_all[4] = {1, 1, 1, 1};
    bit be_none[4] = {0, 0, 0, 0};

    u32_to_word(0xCAFEBABE, wdata);
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);

    // mask=0000 => no change
    u32_to_word(0x00000000, wdata);
    dm.m_write(&dm, addr, wdata, be_none, 1, 1);

    dm.m_read(&dm, addr, rdata);
    ASSERT_EQ_U32("mask=0000 => unchanged", word_to_u32(rdata), 0xCAFEBABE);

    return 0;
}

static int test_unaligned_policy(void) {
    printf("\n=== test_unaligned_policy ===\n");
    Dm_ dm;
    init_dm_(&dm);

    // put known bytes 00 11 22 33 44 55
    for (int i = 0; i < 6; i++) dm.memory[i] = (uint8_t) (i * 0x11);

    word addr = {0}, rdata = {0}, wdata = {0};
    bit be_all[4] = {1, 1, 1, 1};

    // read idx=1
    u32_to_word(1, addr);
    dm.m_read(&dm, addr, rdata);
    uint32_t got = word_to_u32(rdata);

#if DM_ALLOW_UNALIGNED
    ASSERT_EQ_U32("allow: read@1 == 0x11223344", got, 0x11223344);
#else
    ASSERT_EQ_U32("forbid: read@1 == 0", got, 0);
#endif

    // write idx=1 data=0xAABBCCDD
    u32_to_word(1, addr);
    u32_to_word(0xAABBCCDD, wdata);

    uint8_t b1 = dm.memory[1], b2 = dm.memory[2], b3 = dm.memory[3], b4 = dm.memory[4];
    dm.m_write(&dm, addr, wdata, be_all, 1, 1);

#if DM_ALLOW_UNALIGNED
    ASSERT_EQ_U8("allow: mem[1]==AA", dm.memory[1], 0xAA);
    ASSERT_EQ_U8("allow: mem[2]==BB", dm.memory[2], 0xBB);
    ASSERT_EQ_U8("allow: mem[3]==CC", dm.memory[3], 0xCC);
    ASSERT_EQ_U8("allow: mem[4]==DD", dm.memory[4], 0xDD);
#else
    ASSERT_EQ_U8("forbid: mem[1] unchanged", dm.memory[1], b1);
    ASSERT_EQ_U8("forbid: mem[2] unchanged", dm.memory[2], b2);
    ASSERT_EQ_U8("forbid: mem[3] unchanged", dm.memory[3], b3);
    ASSERT_EQ_U8("forbid: mem[4] unchanged", dm.memory[4], b4);
#endif

    return 0;
}

static int test_fuzz_against_golden_model(void) {
    printf("\n=== test_fuzz_against_golden_model ===\n");
    Dm_ dm;
    init_dm_(&dm);

    uint8_t model[DEFAULT_SIZE];
    memset(model, 0, sizeof(model));

    uint32_t seed = 0xC0FFEE01u;

    word addr = {0}, wdata = {0}, rdata = {0};

    for (int it = 0; it < DM_FUZZ_ITERS; it++) {
        uint32_t r = xs32(&seed);

        // Random idx: include some OOB & near-end
        uint32_t idx;
        switch (r & 7u) {
            case 0: idx = DEFAULT_SIZE + (xs32(&seed) % 32);
                break; // OOB
            case 1: idx = (DEFAULT_SIZE - 1) - (xs32(&seed) % 8);
                break; // near end
            default: idx = xs32(&seed) % (DEFAULT_SIZE + 16);
                break; // mix
        }

        // Random data
        uint32_t data = xs32(&seed);

        // Random mask 0..15
        bit be[4];
        mask_from_u4(xs32(&seed) & 0xFu, be);

        // Random we/clk
        bit we = (xs32(&seed) >> 0) & 1u;
        bit clk = (xs32(&seed) >> 1) & 1u;

        // Perform write in both DM and model
        u32_to_word(idx, addr);
        u32_to_word(data, wdata);

        dm.m_write(&dm, addr, wdata, be, we, clk);
        model_write_u32(model, idx, data, be, we, clk);

        // Randomly read and compare
        if ((xs32(&seed) & 3u) == 0) {
            dm.m_read(&dm, addr, rdata);
            uint32_t got = word_to_u32(rdata);
            uint32_t exp = model_read_u32(model, idx);
            if (got != exp) {
                printf("[FAIL] fuzz mismatch at it=%d idx=%u we=%d clk=%d mask=%d%d%d%d\n",
                       it, idx, (int) we, (int) clk, (int) be[0], (int) be[1], (int) be[2], (int) be[3]);
                printf("       got=0x%08X exp=0x%08X data=0x%08X\n",
                       (unsigned) got, (unsigned) exp, (unsigned) data);
                return 1;
            }
        }
    }

    PASS("fuzz matched golden model");
    return 0;
}

// int main(void) {
//     printf("=== TEST: DM Full Regression ===\n");
//     printf("[INFO] Policy: DM_ALLOW_UNALIGNED=%d, DM_TRUNCATE_NEAR_END=%d, FUZZ=%d\n",
//            DM_ALLOW_UNALIGNED, DM_TRUNCATE_NEAR_END, DM_FUZZ_ITERS);
//
//     int rc = 0;
//     rc |= test_basic_rw_and_async_read();
//     rc |= test_write_gating_we_and_clk();
//     rc |= test_mask_zero_is_no_write();
//     rc |= test_mask_all_16_patterns();
//     rc |= test_unaligned_policy();
//     rc |= test_fuzz_against_golden_model();
//
//     if (rc == 0) {
//         printf("\nALL DM FULL TESTS PASSED ✅\n");
//     }
//     return rc;
// }
