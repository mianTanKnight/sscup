//
// Created by wenshen on 2026/1/17.
//
#ifndef SCCPU_DM__H
#define SCCPU_DM__H
#include <unistd.h>
#include <bits/posix2_lim.h>

#include "dff_.h"
#include "stdint.h"
#include "stddef.h"
//  DM 是什么 ?
//  是 CPU Cache 一块储存单元阵列
//  DM 需要做什么?
//  读写
//  怎么读?
//  以 byte 为单位 ->  Addr = 0x00000004
//  Addr=4 意味着第 4 个字节。
//  转换公式： Index = Addr / 4。
//  Addr = 0 -> Index = 0
//  Addr = 4 -> Index = 1
//  Addr = 8 -> Index = 2
// DM 模块允许使用 C 语法
#define DEFAULT_SIZE  1024 * 4

// one byte
typedef struct dm_byte {
    dff_b_ bs[8];
} Dm_byte;

typedef struct dm_ Dm_;

typedef void (*read_fn)(const Dm_ *dm, word addr, size_t size, bit *ret);

typedef void (*write_fn)(const Dm_ *dm, word addr, size_t size, const bit *input, const bit clk);

struct dm_ {
    Dm_byte d[DEFAULT_SIZE]; // 4KB
    read_fn read;
    write_fn write;
};

/**
 * addr -> 从那里开始读
 * size -> 读多少字节
 * ret -> 外部提供的容器 不做安全检查
 */
static inline
void read_(const Dm_ *dm,
           word addr,
           size_t size,
           bit *ret) {
    const uint32_t addr_32 = u32_from_word(addr);
    size = (addr_32 + size >= DEFAULT_SIZE) ? DEFAULT_SIZE - size - 1 : size;
    size_t s = 0;
    for (size_t i = 0; i < size; i++) {
        Dm_byte byte = dm->d[addr_32 + i];
        for (size_t j = 0; j < 8; j++) {
            ret[s++] = byte.bs[j].dff.Q;
        }
    }
}

/**
 * 时序写入
 * addr -> 从那里开始写, 不做写入安全检查
 * size -> 写多少
 * input -> bit级别的输入 不做安全,数据,长度检查
 * clk  -> 二段式时序支持
 */
static inline
void write_(const Dm_ *dm, word addr, size_t size, const bit *input, const bit clk) {
    const uint32_t addr_32 = u32_from_word(addr);
    size = (addr_32 + size >= DEFAULT_SIZE) ? DEFAULT_SIZE - size - 1 : size;
    size_t s = 0;
    for (size_t i = 0; i < size; i++) {
        printf("index %lu\n", addr_32 + i);
        Dm_byte *byte = (Dm_byte *) &dm->d[addr_32 + i];
        for (size_t j = 0; j < 8; j++) {
            dff_deh_step(&byte->bs[j], clk, input[s++]);
        }
    }
}

static inline
void init_dm(Dm_ *dm) {
    for (size_t i = 0; i < DEFAULT_SIZE; i++) {
        for (size_t j = 0; j < 8; j++)
            init_dff_deh(&dm->d[i].bs[j]);
    }
    dm->read = read_;
    dm->write = write_;
}


#endif //SCCPU_DM__H
