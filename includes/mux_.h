//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_MUX__H
#define SCCPU_MUX__H
#include "common_.h"
#include "gate_.h"

static inline bit mux2_1(const bit input0, const bit input1, const bit sel) {
    return OR(AND(NOT(sel), input0), AND(sel, input1));
}

static inline void byte_mux_2_1(const byte input0, const byte input1, const bit sel, byte output) {
    for (int i = 0; i < BYTE_SIZE; i++) output[i] = mux2_1(input0[i], input1[i], sel);
}

// sel 1  -> input1  else input0
static inline void word_mux_2_1(const word input0, const word input1, const bit sel, word output) {
    for (int i = 0; i < WORD_SIZE; i++) output[i] = mux2_1(input0[i], input1[i], sel);
}

#endif //SCCPU_MUX__H
