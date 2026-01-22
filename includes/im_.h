//
// Created by wenshen on 2025/12/10.
//

#ifndef SCCPU_IM__H
#define SCCPU_IM__H
#define IM_SIZE 256
#include "common_.h"
#include "string.h"
#include "utils.h"
// IM 模块允许使用 "高级语法"
// 它本质是指令的储存柜
// 就像我们使用 bool 是表示 bit 而非 dff
// 关注核心 简化边缘

typedef struct {
    word im[IM_SIZE];
} Im_t;

static inline void init_imt(Im_t *imt) {
    memset(imt, 0, sizeof(Im_t));
}

static inline void im_load_program(Im_t *imt, const word *program_codes, const size_t codes_len) {
    const size_t len = codes_len > IM_SIZE ? IM_SIZE : codes_len;
    for (size_t i = 0; i < len; i++) {
        memcpy(imt->im[i], program_codes[i], sizeof(word));
    }
}

static inline void im_read(const Im_t *imt, word pc, word instruction_out) {
    memcpy(instruction_out, imt->im[u32_from_word(pc) / 4], sizeof(word));
}


#endif //SCCPU_IM__H
