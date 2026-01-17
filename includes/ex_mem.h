//
// Created by wenshen on 2026/1/4.
//

#ifndef SCCPU_EX_MEM_H
#define SCCPU_EX_MEM_H
#include "if_id_.h"
#include "id_ex_.h"
#include "alu_.h"

typedef struct ex_mem_regs {
    Reg32_ mem_single; // mem_read, mem_write
    Reg32_ wb_single; // reg_write, data_src_to_reg
    // result by ALU
    // ALU 并不关注结果是数据还是地址 它是由去向决定身份  MEM_READ = 1 表明是内存地址值
    Reg32_ alu_result;
    // write_data 和 alu_result 通常 write_data 并不是ALU 计算出来的结果而是从寄存器读取出来的结果(待写入内存)
    // Data2
    Reg32_ write_data;
    // 目的地 reg_idx_
    Reg32_ write_reg_idx;
} Ex_mem_regs;


static inline void
init_ex_eme_regs(Ex_mem_regs *regs) {
    init_reg32(&regs->mem_single);
    init_reg32(&regs->wb_single);
    init_reg32(&regs->alu_result);
    init_reg32(&regs->write_data);
    init_reg32(&regs->write_reg_idx);
}


/**
 *  hazard 是 ex -> if 的回通电路
 *  If_id_pc_ops 在二段式的第一段(低电平)时被更新 (如果需要)
 *  If_id_pc_ops 中使用的直连式 而非 DFF_
 *  step2
 *  if_id_step(ops);
 *
 */
static inline void
hazard(If_id_pc_ops *ops,
       const pc_ops pc_src, // The head pointer of the array
       word branch_target, // The head pointer of the array
       If_id_write* if_id_write,
       Id_ex_write* id_ex_write
) {
    ops->pc_ops_[0] = pc_src[0];
    ops->pc_ops_[1] = pc_src[1];
    connect(branch_target, ops->branch_target_wire);
    // ifid_write 看起来是被"储存"了
    // 因为 branch_taken 是被实时计算出来的 也就是说 在 step2 阶段 branch_taken 也是会被计算的
    // c->ifid_write.if_id_flush = branch_taken; 更像是连接
    bit branch_taken = AND(pc_src[0], NOT(pc_src[1]));
    if_id_write->if_id_flush = branch_taken;
    id_ex_write->id_ex_flush = branch_taken;

}


static inline void
ex_mem_regs_step(const Id_ex_regs *id_ex_regs,
                 Ex_mem_regs *ex_mem_regs,
                 pc_ops pc_src, // The head pointer of the array
                 word branch_target, // The head pointer of the array
                 const bit ex_flush,
                 bit *overflow,
                 const bit clk) {
    // get alu_src ->  IMM by ALU_SRC is 1  else RT
    bit alu_src = GET_ALU_SRC_OF_SIGNALS(&id_ex_regs->decode_signals);

    word input0_w = {0}, input1_w = {0}, data2_w = {0}, imm_ext_w = {0};
    read_reg32(&id_ex_regs->read_data1, input0_w); // read of R1
    read_reg32(&id_ex_regs->read_data2, data2_w); // read of R2
    read_reg32(&id_ex_regs->imm_ext, imm_ext_w);
    word_mux_2_1(data2_w, imm_ext_w, alu_src, input1_w);

    // alu_ops
    ops alu_ops = {0};
    GET_OPS_OF_SIGNALS(&id_ex_regs->decode_signals, alu_ops);
    // alu_result
    word alu_result_w = {0};
    word_alu_(input0_w, input1_w, alu_result_w, alu_ops, overflow);

    word mem_single_w = {0}, wb_single_w = {0};
    mem_single_w[INST_WORD(31)] = GET_MEM_READ_OF_SIGNALS(&id_ex_regs->decode_signals);
    mem_single_w[INST_WORD(30)] = GET_MEM_WRITE_OF_SIGNALS(&id_ex_regs->decode_signals);

    // wb：reg_write 放 bit31，mem_to_reg 放 bit30
    wb_single_w[INST_WORD(31)] = GET_REG_WRITE_OF_SIGNALS(&id_ex_regs->decode_signals);
    wb_single_w[INST_WORD(30)] = GET_DATA_SRC_TO_REG_OF_SIGNALS(&id_ex_regs->decode_signals);

    // id_ex_regs.pc_plus4 已经是 + 4
    word pc_push4_w = {0};
    read_reg32(&id_ex_regs->pc_plus4, pc_push4_w);
    // 算出 branch_target 注意 branch_target 并不是一个寄存器 而是一个32位的导线 立即性
    word imm_ext_lshift2_w = {0};
    word_lshift2(imm_ext_w, imm_ext_lshift2_w);
    word_alu_(pc_push4_w, imm_ext_lshift2_w, branch_target, OPS_ADD_, overflow);

    word ret_src = {0};
    // R1 - R2
    word_alu_(input0_w, data2_w, ret_src, OPS_SUB_, overflow);
    const bit branch_signal = GET_BRANCH_OF_SIGNALS(&id_ex_regs->decode_signals);
    const bit is_zero = word_is_zero(ret_src);
    // const static pc_ops BRANCH_TARGET = {1, 0};
    // flush 会丢弃指令 当然也会丢弃当条 ?
    pc_src[0] = AND(AND(branch_signal, is_zero), NOT(ex_flush));
    // pc_src[1] default zero
    pc_src[1] = 0;

    bit reg_dst = GET_REG_DST_OF_SIGNALS(&id_ex_regs->decode_signals);
    word write_final_reg_idx = {0};
    word rt_idx_w = {0}, rd_idx_w = {0};
    read_reg32(&id_ex_regs->rt_idx, rt_idx_w);
    read_reg32(&id_ex_regs->rd_idx, rd_idx_w);
    word_mux_2_1(rt_idx_w, rd_idx_w, reg_dst, write_final_reg_idx);

    word mem_single_in = {0};
    word_mux_2_1(mem_single_w, WORD_ZERO, ex_flush, mem_single_in);
    word wb_single_in = {0};
    word_mux_2_1(wb_single_w, WORD_ZERO, ex_flush, wb_single_in);
    word alu_result_in = {0};
    word_mux_2_1(alu_result_w, WORD_ZERO, ex_flush, alu_result_in);
    word write_data_in = {0};
    word_mux_2_1(data2_w, WORD_ZERO, ex_flush, write_data_in);
    word write_reg_idx_in = {0};
    word_mux_2_1(write_final_reg_idx, WORD_ZERO, ex_flush, write_reg_idx_in);

    word out = {0};
    reg32_step(&ex_mem_regs->mem_single, 1, mem_single_in, out, clk);
    reg32_step(&ex_mem_regs->wb_single, 1, wb_single_in, out, clk);
    reg32_step(&ex_mem_regs->alu_result, 1, alu_result_in, out, clk);
    reg32_step(&ex_mem_regs->write_data, 1, write_data_in, out, clk);
    reg32_step(&ex_mem_regs->write_reg_idx, 1, write_reg_idx_in, out, clk);
}

#endif //SCCPU_EX_MEM_H
