//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_GATE__H
#define SCCPU_GATE__H
#include "common.h"

static inline bit NOT(const bit input) {
    return !input;
}

static inline bit AND(const bit input1, const bit input2) {
    return input1 & input2;
}

static inline bit OR(const bit input1, const bit input2) {
    return input1 | input2;
}

static inline bit NAND(const bit input1, const bit input2) {
    return NOT(AND(input1, input2));
}

static inline bit NOR(const bit input1, const bit input2) {
    return NOT(OR(input1, input2));
}

static inline bit XOR(const bit input1, const bit input2) {
    return AND(OR(input1, input2), NAND(input1, input2));
}


static inline bit XNOR(const bit input1, const bit input2) {
    return NOT(XOR(input1, input2));
}


#endif //SCCPU_GATE__H
