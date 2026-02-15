//
// Created by wenshen on 2026/1/4.
//

#ifndef SCCPU_EX_MEM_H
#define SCCPU_EX_MEM_H
#include "if_id.h"
#include "id_ex.h"
#include "alu.h"
#include "utils.h"

/**
 * Ex 在真正进行ALU 会对 input0,input1 和 forwarding_unit_writes
 * 做选择
 * Cpu_Core 在判断存在 forwarding_unit 情况时间会使 enable 为 1
 */
typedef struct forwarding_unit_writes {
    // 三个输入回线 input0, input1, sw
    word input0_forwarding_write;
    bit i0_enable;
    word input1_forwarding_write;
    bit i1_enable;
    word sw_forwarding_write;
    bit sw_enable;
} Forwarding_unit_writes;

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
init_ex_mem_regs(Ex_mem_regs *regs) {
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
 *
 *  在调用 hazard 时
 *  ops, pc_src, If_id_write.if_id_flush,id_ex_write->id_ex_flush
 *  应该被清0
 *
 */
static inline void
hazard(If_id_pc_ops *ops,
       const pc_ops pc_src, // The head pointer of the array
       word branch_target, // The head pointer of the array
       If_id_write *if_id_write,
       Id_ex_write *id_ex_write
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
forwarding_mux(const Forwarding_unit_writes fuw, word input0, word input1) {
    word_mux_2_1(input0, fuw.input0_forwarding_write,
                 fuw.i0_enable, input0);
    word_mux_2_1(input1, fuw.input1_forwarding_write,
                 fuw.i1_enable, input1);
}

static inline void forwarding_mux_sw(const Forwarding_unit_writes fuw,
                                     const Id_ex_regs *id_ex_regs,
                                     word data2_w /*inout*/) {
    // get mem_write state
    const bit is_store = GET_MEM_WRITE_OF_SIGNALS(&id_ex_regs->decode_signals);
    word_mux_2_1(data2_w, fuw.sw_forwarding_write,
                 AND(is_store, fuw.sw_enable), data2_w);
}


static inline
void ex_mem_regs_step(const Id_ex_regs *id_ex_regs,
                       Ex_mem_regs *ex_mem_regs,
                       const Forwarding_unit_writes *fuw,
                       pc_ops pc_src,
                       word branch_target,
                       const bit ex_flush,
                       bit *overflow,
                       const bit clk
) {
    // =========================================================================
    // EX Stage (Combinational Wires)
    // =========================================================================
    // clk = 0 ; 计算wire 准备 D端
    // clk =1  ; 更新 Q

    // Read Control Bits (from ID/EX decode_signals)
    const bit alu_src = GET_ALU_SRC_OF_SIGNALS(&id_ex_regs->decode_signals);   // 1: imm, 0: rt
    const bit reg_dst = GET_REG_DST_OF_SIGNALS(&id_ex_regs->decode_signals);   // 1: rd, 0: rt
    const bit mem_read  = GET_MEM_READ_OF_SIGNALS(&id_ex_regs->decode_signals);
    const bit mem_write = GET_MEM_WRITE_OF_SIGNALS(&id_ex_regs->decode_signals);
    const bit reg_write = GET_REG_WRITE_OF_SIGNALS(&id_ex_regs->decode_signals);
    const bit mem_to_reg= GET_DATA_SRC_TO_REG_OF_SIGNALS(&id_ex_regs->decode_signals);
    const bit branch    = GET_BRANCH_OF_SIGNALS(&id_ex_regs->decode_signals);

    // ALU ops
    ops alu_ops = {0};
    GET_OPS_OF_SIGNALS(&id_ex_regs->decode_signals, alu_ops);

    // -----------------------------
    // 1) Read Data Wires (from ID/EX pipeline regs)
    // -----------------------------
    // rs_val / rt_val：这是“寄存器读出来的原始值”
    // imm_ext：符号扩展后的立即数（不需要 forwarding）
    word rs_val  = {0};
    word rt_val  = {0};
    word imm_ext = {0};
    read_reg32(&id_ex_regs->read_data1, rs_val);
    read_reg32(&id_ex_regs->read_data2, rt_val);
    read_reg32(&id_ex_regs->imm_ext,    imm_ext);

    // -----------------------------
    // 2) Apply Forwarding (only for register operands)
    // -----------------------------
    // forwarding 的语义是“把最新值劫持回 EX 的输入端”
    // 我们对 rs_val / rt_val 做劫持（rt_val 代表寄存器 RT 的值）
    forwarding_mux(*fuw, rs_val, rt_val);

    // SW 的写数据也是 RT，但它有独立的 sw_forwarding 通路（load/use 等未来扩展更清晰）
    forwarding_mux_sw(*fuw, id_ex_regs, rt_val);

    // -----------------------------
    // 3) Build ALU inputs
    // -----------------------------
    // ALU_A = rs_val
    // ALU_B = mux(rt_val, imm_ext, alu_src)
    word alu_b = {0};
    word_mux_2_1(rt_val, imm_ext, alu_src, alu_b);

    // -----------------------------
    // 4) ALU Execution
    // -----------------------------
    word alu_result_w = {0};
    word_alu_(rs_val, alu_b, alu_result_w, alu_ops, overflow);

    // -----------------------------
    // 5) Branch Target & Branch Decision (BEQ)
    // -----------------------------
    // branch_target = pc_plus4 + (imm_ext << 2)
    word pc_plus4 = {0};
    read_reg32(&id_ex_regs->pc_plus4, pc_plus4);

    word imm_lshift2 = {0};
    word_lshift2(imm_ext, imm_lshift2);
    word_alu_(pc_plus4, imm_lshift2, branch_target, OPS_ADD_, overflow);

    // BEQ compare 必须用“寄存器对”(rs_val vs rt_val)，且这两个值已经吃过 forwarding
    word diff = {0};
    word_alu_(rs_val, rt_val, diff, OPS_SUB_, overflow);
    const bit is_zero = word_is_zero(diff);

    // pc_src: 10 表示 branch
    // ex_flush=1 时必须压掉分支（异常/kill 场景），否则会错误改 PC
    pc_src[0] = AND(AND(branch, is_zero), NOT(ex_flush));
    pc_src[1] = 0;

    // -----------------------------
    // 6) Build EX/MEM control bundles + destination register index
    // -----------------------------
    // mem_single: bit31=mem_read, bit30=mem_write
    word mem_single_w = {0};
    mem_single_w[INST_WORD(31)] = mem_read;
    mem_single_w[INST_WORD(30)] = mem_write;

    // wb_single: bit31=reg_write, bit30=mem_to_reg
    word wb_single_w = {0};
    wb_single_w[INST_WORD(31)] = reg_write;
    wb_single_w[INST_WORD(30)] = mem_to_reg;

    // dest reg idx = mux(rt_idx, rd_idx, reg_dst)
    word rt_idx = {0}, rd_idx = {0}, dst_idx = {0};
    read_reg32(&id_ex_regs->rt_idx, rt_idx);
    read_reg32(&id_ex_regs->rd_idx, rd_idx);
    word_mux_2_1(rt_idx, rd_idx, reg_dst, dst_idx);

    // write_data: 只对 SW 有意义，但即使对非 SW，mem_write=0 也无副作用
    // 我们把“forwarded 后的 rt_val”作为 store 数据送入 EX/MEM
    word write_data_w = {0};
    connect(rt_val, write_data_w);


    // -----------------------------
    // 7) Apply ex_flush squash (kill side effects)
    // -----------------------------
    // ex_flush 时必须保证：写寄存器 / 写内存 / 读内存 等副作用都被清掉
    // 控制信号清零，数据也可以清零（便于调试）
    word mem_single_in   = {0};
    word wb_single_in    = {0};
    word alu_result_in   = {0};
    word write_data_in   = {0};
    word write_reg_idx_in= {0};

    word_mux_2_1(mem_single_w,    WORD_ZERO, ex_flush, mem_single_in);
    word_mux_2_1(wb_single_w,     WORD_ZERO, ex_flush, wb_single_in);
    word_mux_2_1(alu_result_w,    WORD_ZERO, ex_flush, alu_result_in);
    word_mux_2_1(write_data_w,    WORD_ZERO, ex_flush, write_data_in);
    word_mux_2_1(dst_idx,         WORD_ZERO, ex_flush, write_reg_idx_in);

    // =========================================================================
    // EX/MEM Pipeline Registers Latch (Sequential)
    // =========================================================================
    word out = {0};
    reg32_step(&ex_mem_regs->mem_single,    1, mem_single_in,    out, clk);
    reg32_step(&ex_mem_regs->wb_single,     1, wb_single_in,     out, clk);
    reg32_step(&ex_mem_regs->alu_result,    1, alu_result_in,    out, clk);
    reg32_step(&ex_mem_regs->write_data,    1, write_data_in,    out, clk);
    reg32_step(&ex_mem_regs->write_reg_idx, 1, write_reg_idx_in, out, clk);

}

#endif //SCCPU_EX_MEM_H
