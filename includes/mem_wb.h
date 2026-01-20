//
// Created by wenshen on 2026/1/20.
//

#ifndef SCCPU_MEM_WB_H
#define SCCPU_MEM_WB_H
#include "reg_.h"
#include "common_.h"
#include "ex_mem.h"
#include "dm_.h"

typedef struct mem_wb_regs {
    Reg32_ wb_single; // penetrate
    Reg32_ mem_read_data; // from dm
    Reg32_ alu_result; // penetrate
    Reg32_ write_reg_idx; // penetrate
} Mem_wb_regs;


static inline
void init_mem_wb_regs(Mem_wb_regs *mem_wb_regs) {
    init_reg32(&mem_wb_regs->wb_single);
    init_reg32(&mem_wb_regs->mem_read_data);
    init_reg32(&mem_wb_regs->alu_result);
    init_reg32(&mem_wb_regs->write_reg_idx);
}


static inline
void mem_wb_regs_step(const Ex_mem_regs *ex_mem_regs,
                      Mem_wb_regs *mem_wb_regs,
                      Dm_ *dm,
                      const bit writer_enabled[4],
                      const bit clk) {
    // 我们可以无视 mem_read 不管怎么样我们都会尝试去读 因为读是无害 使用才是有害的
    // 如果使用 if 我们会破坏电路性
    // bit mem_read = GET_BIT_OF_REG32(&ex_mem_regs->mem_single, 31);
    const bit mem_writer = GET_BIT_OF_REG32(&ex_mem_regs->mem_single, 30);
    word alu_result = {0}, write_data = {0}, wb_single = {0}, write_reg_idx = {0};
    read_reg32(&ex_mem_regs->alu_result, alu_result);
    read_reg32(&ex_mem_regs->write_data, write_data);
    read_reg32(&ex_mem_regs->wb_single, wb_single);
    read_reg32(&ex_mem_regs->write_reg_idx, write_reg_idx);

    word read_ret = {0};
    dm->m_read(dm, alu_result, read_ret);

    //写由 we 控制
    dm->m_write(dm, alu_result, write_data, writer_enabled, mem_writer, clk);

    word out = {0};
    reg32_step(&mem_wb_regs->wb_single, 1, wb_single, out, clk);
    reg32_step(&mem_wb_regs->mem_read_data, 1, read_ret, out, clk);
    reg32_step(&mem_wb_regs->alu_result, 1, alu_result, out, clk);
    reg32_step(&mem_wb_regs->write_reg_idx, 1, write_reg_idx, out, clk);
}


#endif //SCCPU_MEM_WB_H
