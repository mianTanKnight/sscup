//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_COMMON__H
#define SCCPU_COMMON__H
#define BYTE_SIZE 8
#define WORD_SIZE 32

typedef _Bool bit;
typedef bit byte[BYTE_SIZE];
typedef bit word[WORD_SIZE];

// word[0]=MSB

const static byte BYTE_ONE = {0, 0, 0, 0, 0, 0, 0, 1};
const static byte BYTE_ZERO = {0};
const static word WORD_ONE = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};
const static word NOP = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
const static word ALL1 = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
const static word WORD_ZERO = {0};


// 逻辑 bit n (31..0) -> 存储下标 index = 31 - n
#define INST_BIT(instr, n)  ((instr)[WORD_SIZE - 1 - (n)])
#define INST_WORD(n) (WORD_SIZE - 1 - (n))

#endif //SCCPU_COMMON__H
