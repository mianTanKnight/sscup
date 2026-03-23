//
// common_test.h - Unified test infrastructure for SCCPU
// All test files should include this as their single test utility header.
//

#ifndef SCCPU_COMMON_TEST_H
#define SCCPU_COMMON_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Production headers (utils.h pulls in common.h, reg.h, isa.h, etc.)
#include "../includes/utils.h"

// ======================== Test Result Tracking ========================

static int _test_pass_count = 0;
static int _test_fail_count = 0;

#define PASS(msg) do { \
    printf("[PASS] %s\n", (msg)); \
    _test_pass_count++; \
} while (0)

#define FAIL(msg) do { \
    printf("[FAIL] %s\n", (msg)); \
    _test_fail_count++; \
    return 1; \
} while (0)

#define ASSERT_EQ_U32(name, actual, expected) do { \
    uint32_t _a = (uint32_t)(actual); \
    uint32_t _e = (uint32_t)(expected); \
    if (_a != _e) { \
        printf("[FAIL] %s: got=0x%08X (%u), expected=0x%08X (%u)\n", \
               (name), _a, _a, _e, _e); \
        _test_fail_count++; \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
        _test_pass_count++; \
    } \
} while (0)

#define ASSERT_EQ_BIT(name, actual, expected) do { \
    int _a = ((actual) & 1); \
    int _e = ((expected) & 1); \
    if (_a != _e) { \
        printf("[FAIL] %s: got=%d, expected=%d\n", (name), _a, _e); \
        _test_fail_count++; \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
        _test_pass_count++; \
    } \
} while (0)

#define ASSERT_EQ_U8(name, actual, expected) do { \
    uint8_t _a = (uint8_t)(actual); \
    uint8_t _e = (uint8_t)(expected); \
    if (_a != _e) { \
        printf("[FAIL] %s: got=0x%02X, expected=0x%02X\n", (name), _a, _e); \
        _test_fail_count++; \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
        _test_pass_count++; \
    } \
} while (0)

#define TEST_SUMMARY(suite_name) do { \
    printf("\n========== %s ==========\n", (suite_name)); \
    printf("  Passed: %d\n", _test_pass_count); \
    printf("  Failed: %d\n", _test_fail_count); \
    if (_test_fail_count == 0) \
        printf("  Result: ALL PASSED\n"); \
    else \
        printf("  Result: SOME FAILED\n"); \
    printf("====================================\n"); \
} while (0)

// ======================== Convenience Helpers ========================
// These are guarded by include checks so common_test.h works for all tests.

#ifdef SCCPU_PC__H
static inline void read_pc_u32(Pc32_ *pc, uint32_t *out) {
    *out = reg32_read_u32(&pc->reg32);
}
#endif

#ifdef SCCPU_IF_ID__H
static inline void read_ifid_instr_u32(If_id_regs *r, uint32_t *out) {
    *out = reg32_read_u32(&r->instr);
}

static inline void read_ifid_pc4_u32(If_id_regs *r, uint32_t *out) {
    *out = reg32_read_u32(&r->pc_plus4);
}
#endif

#ifdef SCCPU_IM__H
static inline void im_set_u32(Im_t *im, uint32_t index, uint32_t inst) {
    word_from_u32(inst, im->im[index]);
}
#endif

// Access bit n (31..0) from a word
static inline bit BITN(const word w, int n) {
    return INST_BIT(w, n) & 1;
}

// Print word as hex
static inline void print_word_hex(const word w) {
    printf("0x%08X", (unsigned)word_to_u32(w));
}

// ======================== Run Test Helper ========================
// Usage: RUN_TEST(test_function_name);
#define RUN_TEST(fn) do { \
    int _rc = fn(); \
    if (_rc) { \
        printf("[SUITE] %s: FAILED\n", #fn); \
        rc |= _rc; \
    } \
} while (0)

#endif //SCCPU_COMMON_TEST_H
