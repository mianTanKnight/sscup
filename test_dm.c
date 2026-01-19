//
// Created by wenshen on 2026/1/19.
//
#include <stdio.h>
#include "includes/common_.h"
#include "includes/dm_.h"

// --------------------- 辅助函数 ---------------------

// 把 u32 转成 word (MSB first)
void u32_to_word(uint32_t val, word w) {
    for(int i=0; i<32; i++) {
        w[i] = (val >> (31-i)) & 1;
    }
}

// 把 word 转成 u32
uint32_t word_to_u32(const word w) {
    uint32_t val = 0;
    for(int i=0; i<32; i++) {
        if(w[i]) val |= (1u << (31-i));
    }
    return val;
}

// 断言宏
#define ASSERT_EQ_HEX(msg, actual, expected) do { \
    if((actual) != (expected)) { \
        printf("[FAIL] %s: Got 0x%08X, Expected 0x%08X\n", msg, (actual), (expected)); \
        return 1; \
    } else { \
        printf("[PASS] %s\n", msg); \
    } \
} while(0)

// --------------------- 测试用例 ---------------------

// int main() {
//     printf("=== TEST: Data Memory (DM) ===\n");
//
//     Dm_ dm;
//     init_dm_(&dm);
//
//     word addr = {0};
//     word wdata = {0};
//     word rdata = {0};
//     bit mask_all[4] = {1, 1, 1, 1}; // 全写
//     bit mask_lsb[4] = {0, 0, 0, 1}; // 只写最低字节
//     bit mask_msb[4] = {1, 0, 0, 0}; // 只写最高字节
//
//     // 1. 基础读写测试 (Word Access)
//     printf("\n--- Test 1: Basic Word R/W ---\n");
//     u32_to_word(0x00000004, addr);      // Addr = 4
//     u32_to_word(0xDEADBEEF, wdata);     // Data = 0xDEADBEEF
//
//     // Write (Sync)
//     dm.m_write(&dm, addr, wdata, mask_all, 1, 1);
//
//     // Read (Async)
//     dm.m_read(&dm, addr, rdata);
//     ASSERT_EQ_HEX("Read back DEADBEEF", word_to_u32(rdata), 0xDEADBEEF);
//
//     // 2. 时序测试 (Clock Gating)
//     printf("\n--- Test 2: Timing (Clock) ---\n");
//     u32_to_word(0x00000008, addr);      // Addr = 8
//     u32_to_word(0x12345678, wdata);
//
//     // Try write with clk=0 (Should FAIL to write)
//     dm.m_write(&dm, addr, wdata, mask_all, 1, 0);
//
//     dm.m_read(&dm, addr, rdata);
//     ASSERT_EQ_HEX("Read back 0 (Write should not happen when clk=0)", word_to_u32(rdata), 0x00000000);
//
//     // Write with clk=1 (Should SUCCEED)
//     dm.m_write(&dm, addr, wdata, mask_all, 1, 1);
//     dm.m_read(&dm, addr, rdata);
//     ASSERT_EQ_HEX("Read back 0x12345678 (Write happened when clk=1)", word_to_u32(rdata), 0x12345678);
//
//     // 3. 字节使能测试 (Byte Enable / Masking)
//     printf("\n--- Test 3: Byte Enable Mask ---\n");
//     u32_to_word(0x00000010, addr);      // Addr = 16
//
//     // Step A: 初始化为全 F
//     u32_to_word(0xFFFFFFFF, wdata);
//     dm.m_write(&dm, addr, wdata, mask_all, 1, 1);
//
//     // Step B: 只写最低字节为 0x00 (Mask=0001)
//     // Data = 0x00000000 (但只有最低位会被采纳)
//     u32_to_word(0x00000000, wdata);
//     dm.m_write(&dm, addr, wdata, mask_lsb, 1, 1);
//
//     // Expect: 0xFFFFFF00
//     dm.m_read(&dm, addr, rdata);
//     ASSERT_EQ_HEX("Mask LSB (Expected FFFFFF00)", word_to_u32(rdata), 0xFFFFFF00);
//
//     // Step C: 只写最高字节为 0xAA (Mask=1000)
//     // Data = 0xAAxxxxxx
//     u32_to_word(0xAA000000, wdata);
//     dm.m_write(&dm, addr, wdata, mask_msb, 1, 1);
//
//     // Expect: 0xAAFFFF00
//     dm.m_read(&dm, addr, rdata);
//     ASSERT_EQ_HEX("Mask MSB (Expected AAFFFF00)", word_to_u32(rdata), 0xAAFFFF00);
//
//     // 4. 越界测试
//     printf("\n--- Test 4: Out of Bounds ---\n");
//     // Addr = Size (Just Out)
//     u32_to_word(DEFAULT_SIZE, addr);
//     u32_to_word(0xBADF00D, wdata);
//
//     dm.m_write(&dm, addr, wdata, mask_all, 1, 1);
//     dm.m_read(&dm, addr, rdata);
//     ASSERT_EQ_HEX("OOB Read (Should be 0)", word_to_u32(rdata), 0x00000000);
//
//     return 0;
// }