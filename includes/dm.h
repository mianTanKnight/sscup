//
// Created by wenshen on 2026/1/17.
//
#ifndef SCCPU_DM__H
#define SCCPU_DM__H
#include "string.h"
#include "stdint.h"
#include "common.h"
#include "utils.h"

//dm 是属于内存模块,项目中的内存模块都应该以简单和实用实现,允许使用C语法和硬件黑盒性
//dm 的读写标准, dm的输出与输入是以32bit单位为标准的
//采用禁止非对齐策略
#define DEFAULT_SIZE  (1024 * 4)
#define GET_BIT_UINT8(x,i) ((bit)((((uint8_t)(x)) >> (i)) & 1u))
typedef struct dm_ Dm_;

// word is array type
typedef bit (*dm_read_fn)(Dm_ *dm, word address, word ret);

typedef bit (*dm_write_fn)(Dm_ *dm, word address, word data, const bit byte_enable_mask[4], const bit we,
                           const bit clk);

struct dm_ {
    uint8_t memory[DEFAULT_SIZE]; // 4KB
    dm_read_fn m_read;
    dm_write_fn m_write;
};

static inline
bit dm_read(Dm_ *dm, word address, word ret) {
    memset(ret, 0, sizeof(word));
    const uint32_t idx = u32_from_word(address);
    if ((idx & 3) != 0) return 1;
    if (idx >= DEFAULT_SIZE) return 1;
    const size_t len = (idx + 4 > DEFAULT_SIZE) ? (DEFAULT_SIZE - idx) : 4;
    size_t s = 0;
    for (size_t i = 0; i < len; i++) {
        const uint8_t u = dm->memory[idx + i];
        for (int j = 7; j >= 0; j--) {
            // 需要倒读
            ret[s++] = GET_BIT_UINT8(u, j);
        }
    }
    return 0;
}

static inline
bit dm_write(Dm_ *dm, word address, word data, const bit byte_enable_mask[4], const bit we,
             const bit clk) {
    if (we & clk) {
        const uint32_t idx = u32_from_word(address);
        if ((idx & 3) != 0) return 1;
        if (idx >= DEFAULT_SIZE) return 1;
        uint8_t d_4int8[4] = {0};
        u32_from_4byte(data, d_4int8);
        const size_t len = (idx + 4 > DEFAULT_SIZE) ? (DEFAULT_SIZE - idx) : 4;
        for (size_t i = 0; i < len; i++) {
            if (byte_enable_mask[i])
                dm->memory[idx + i] = d_4int8[i];
        }
    }
    return 0;
}

static inline
void init_dm_(Dm_ *dm) {
    memset(dm->memory, 0, DEFAULT_SIZE);
    dm->m_read = dm_read;
    dm->m_write = dm_write;
}


#endif //SCCPU_DM__H
