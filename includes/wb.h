//
// Created by wenshen on 2026/1/20.
//

#ifndef SCCPU_WB__H
#define SCCPU_WB__H

#include "common.h"
#include "reg.h"
#include "mem_wb.h"


static inline void
wb_step(const Mem_wb_regs *mw,
        Reg324file_ *rf,
        const bit clk) {
    const bit reg_write = GET_BIT_OF_REG32(&mw->wb_single, 31);
    const bit mem_to_reg = GET_BIT_OF_REG32(&mw->wb_single, 30);

    word mem_data = {0}, alu_res = {0}, wdata = {0};
    read_reg32(&mw->mem_read_data, mem_data);
    read_reg32(&mw->alu_result, alu_res);

    word_mux_2_1(alu_res, mem_data, mem_to_reg, wdata);

    // rs_index[INST_WORD(0)] = LSB (bit 0)
    // rs_index[INST_WORD(1)] = MSB (bit 1)
    const bit idx_lsb = GET_BIT_OF_REG32(&mw->write_reg_idx, 0);
    const bit idx_msb = GET_BIT_OF_REG32(&mw->write_reg_idx, 1);

    // idx_msb, idx_lsb -> 00, 01, 10, 11
    const bit we0 = AND(reg_write, AND(NOT(idx_msb), NOT(idx_lsb))); // 00
    const bit we1 = AND(reg_write, AND(NOT(idx_msb), idx_lsb)); // 01
    const bit we2 = AND(reg_write, AND(idx_msb, NOT(idx_lsb))); // 10
    const bit we3 = AND(reg_write, AND(idx_msb, idx_lsb)); // 11

    word out = {0};
    reg32_step(&rf->r0, we0, wdata, out, clk);
    reg32_step(&rf->r1, we1, wdata, out, clk);
    reg32_step(&rf->r2, we2, wdata, out, clk);
    reg32_step(&rf->r3, we3, wdata, out, clk);
}

#endif //SCCPU_WB__H
