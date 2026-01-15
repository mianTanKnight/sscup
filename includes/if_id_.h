//
// Created by wenshen on 2025/12/22.
//

#ifndef SCCPU_IF_ID__H
#define SCCPU_IF_ID__H
#include "common_.h"
#include "reg_.h"
#include "im_.h"
#include "pc_.h"

typedef bit pc_ops[2];
const static pc_ops PLUSH_4 = {0, 0};
const static pc_ops BRANCH_TARGET = {1, 0};
const static pc_ops JUMP_TARGET = {0, 1};
const static pc_ops EXCEPTION_VECTOR = {1, 1};

typedef struct if_id_regs {
    Reg32_ instr;
    Reg32_ pc_plus4;
} If_id_regs;

typedef struct if_id_pc_ops {
    pc_ops pc_ops_;
    word branch_target_wire;
    word jump_target_wire;
    word exception_vector_wire;
} If_id_pc_ops;

typedef struct if_id_write {
    bit pc_write;
    bit if_id_write;
    bit if_id_flush;
} If_id_write;


static inline void
init_if_id_regs(If_id_regs *regs) {
    init_reg32(&regs->instr);
    init_reg32(&regs->pc_plus4);
}

/**
 * IF/ID 流水线寄存器一步（取指 IF + IF/ID 锁存），严格遵守项目的“二段式时钟”电路风格。
 *
 * ============================================================================
 * 【这个函数在做什么】
 * ============================================================================
 * 它同时扮演两件事：
 *  1) IF 组合逻辑：用“当前 PC”去指令存储器 IM 取出指令，并计算 PC+4、PC_next 等“导线值(wire)”
 *  2) IF/ID 时序逻辑：在时钟上沿，把 IF 阶段算好的 wire 锁存到 IF/ID 寄存器（instr / pc_plus4）
 *     并在需要时更新 PC 寄存器本身。
 *
 * 注意：你整个项目的核心纪律是：
 *  - clk = 0：只允许“准备数据/组合逻辑计算”(wire 变化)，不允许“写入寄存器状态”
 *  - clk = 1（上沿触发那一瞬）：才允许“寄存器写入/状态更新”
 *
 * reg32_step(...) 就是“时序器件”的入口；它会在 0->1 上沿时真正更新 Q。
 *
 * ============================================================================
 * 【输入/输出说明】
 * ============================================================================
 * @if_id_regs : IF/ID 流水线寄存器（时序器件）
 *   - instr    : 给 ID 阶段使用的指令字（锁存后的 instruction）
 *   - pc_plus4 : 给 ID 阶段使用的 PC+4（锁存后的 pc_plus4）
 *
 * @imt        : 指令存储器（IM），按 PC 地址读取 32-bit 指令
 *
 * @pc         : PC 寄存器（时序器件），本函数会根据 pc_write 决定是否更新它
 *
 * @pc_ops     : PC 选择控制与目标地址输入（相当于“PC 输入端前面的多路选择器控制”）
 *   pc_ops_->pc_ops_ 是 3bit 编码（你当前的语义）：
 *      00 -> 顺序执行：pc_next = pc_plus4_wire
 *      10 -> 分支：    pc_next = branch_target_wire
 *      01 -> 跳转：    pc_next = jump_target_wire
 *      11 -> 异常：    pc_next = exception_vector
 *   备注：现在使用的是“级联 mux 链”，语义上会形成一个优先级：
 *      exception > jump > branch > plus4
 *   更严格的做法是：外部保证同一时刻只有一个选择为真（one-hot / mutually exclusive）。
 *
 * @write_     : 写使能/冲刷流水控制（这是流水线 hazard 控制的核心）
 *   - pc_write    : PC 是否允许写入（stall 取指时为 0）
 *   - if_id_write : IF/ID 是否允许写入（stall 解码时为 0）
 *   - if_id_flush : IF/ID 是否冲刷（把 IF/ID 注入 NOP，用于分支/跳转错误取指等情况）
 *
 * @overflow   : PC+4 的加法器进位/溢出（主要用于调试或以后扩展）
 *
 * @clk        : 全局时钟电平（0 或 1），由外部以两段式调用：
 *   clk=0 调一次（准备阶段） + clk=1 调一次（提交阶段）
 *
 * ============================================================================
 * 【关键导线(wire)与“写入(write)”概念】
 * ============================================================================
 * 1) old_pc：
 *    - 从 PC 寄存器 Q 端读出来的“当前 PC”（稳定输出）
 *
 * 2) instr_wire：
 *    - 用 old_pc 从 IM 读出来的“当前指令”（组合逻辑导线值）
 *
 * 3) pc_plus4_wire：
 *    - old_pc + 4 的结果（组合逻辑导线值）
 *    - 它属于“当前这条指令的伴随信息”，要送进 IF/ID 锁存，给后续阶段做分支地址计算等。
 *
 * 4) pc_next_wire：
 *    - 这是“真正要写入 PC 寄存器 D 端”的下一条取指地址（组合逻辑导线值）
 *    - 它是 pc_plus4_wire 和 branch/jump/exception 目标经过 mux 选择后的结果。
 *
 * 重点：pc_plus4_wire 和 pc_next_wire 必须区分！
 *    - pc_plus4_wire：绑定“当前指令”，要进入流水线寄存器（IF/ID.pc_plus4）
 *    - pc_next_wire ：绑定“下一次取指”，要进入 PC 寄存器（PC.D）
 *
 * ============================================================================
 * 【stall / flush 行为契约（非常重要）】
 * ============================================================================
 * 1) PC 写控制：
 *   - pc_write = 0：PC 保持不变（停止取指推进）
 *   - pc_write = 1：PC 在上沿更新为 pc_next_wire
 *
 * 2) IF/ID 写控制：
 *   - if_id_write = 0 且 if_id_flush = 0：IF/ID 保持不变（停止 ID 推进）
 *   - if_id_flush = 1：IF/ID 在上沿写入 NOP（消除错误路径指令的副作用）
 *   - if_id_write = 1：IF/ID 在上沿写入正常的 instr_wire 与 pc_plus4_wire
 *
 * 3) NOP 的“decoder 安全性”：
 *   - 选择 NOP=全 0（0x00000000）是常见做法
 *   - 但必须保证 decode(0) 输出是“无副作用控制信号”：
 *       reg_write=0, mem_read=0, mem_write=0, branch=0, jump=0
 *   - 若 decode(0) 会误识别成某种有效指令（比如某些实现把它当 sll $0,$0,0），
 *     也必须仍然是无副作用（写 $0 也无效等）。你这里没有 $zero 寄存器概念，
 *     所以更要确保控制信号为 0。
 *
 * ============================================================================
 * 【实现层面的提示（保持电路风格）】
 * ============================================================================
 * - 本函数内部先算 wire（组合逻辑），再调用 reg32_step（时序写入）。
 * - 外部每个周期应当严格调用两次：
 *      if_id_regs_step(..., clk=0);
 *      if_id_regs_step(..., clk=1);
 *   符合“准备/提交”二段式模型。
 */

static inline void
if_id_regs_step(
    If_id_regs *if_id_regs,
    const Im_t *imt,
    Pc32_ *pc,
    const If_id_pc_ops *pc_ops,
    const If_id_write *write_,
    bit *overflow,
    const bit clk
) {
    // IF_Comb
    word old_pc = {0};
    read_reg32(&pc->reg32, old_pc);

    word instr_wire = {0};
    // 在硬件中 计算是并行的
    // pc_plus4_wire = pc + 4 , pc_next_wire = mux(....)
    word pc_plus4_wire = {0};
    word pc_next_wire = {0};

    im_read(imt, old_pc, instr_wire);
    word_alu_(old_pc, WORD_4_BYTE, pc_plus4_wire, OPS_ADD_, overflow);

    bit sel_btw = AND(pc_ops->pc_ops_[0], NOT(pc_ops->pc_ops_[1])); // 10
    bit sel_jtw = AND(NOT(pc_ops->pc_ops_[0]), pc_ops->pc_ops_[1]); //01
    bit sel_ev = AND(pc_ops->pc_ops_[0], pc_ops->pc_ops_[1]); // 11

    word_mux_2_1(pc_plus4_wire, pc_ops->branch_target_wire, sel_btw, pc_next_wire);
    word_mux_2_1(pc_next_wire, pc_ops->jump_target_wire, sel_jtw, pc_next_wire);
    word_mux_2_1(pc_next_wire, pc_ops->exception_vector_wire, sel_ev, pc_next_wire);


    //ID
    //out is ignored
    word out = {0};
    // pc
    reg32_step(&pc->reg32, write_->pc_write, pc_next_wire, out, clk);

    // NOP -> 32 Zero
    word instr_ops = {0};
    // NOP 会把指令刷成 0 (DeCode已做无害化支持)
    // pc_plus4_wire 不需要 刷成0
    // 例如 pc -> 0x100  pc_plus_4_wire 依然是 0x104
    word_mux_2_1(instr_wire, NOP, write_->if_id_flush, instr_ops);

    reg32_step(&if_id_regs->instr, OR(write_->if_id_write, write_->if_id_flush), instr_ops, out, clk);
    reg32_step(&if_id_regs->pc_plus4, OR(write_->if_id_write, write_->if_id_flush), pc_plus4_wire, out, clk);
}


#endif //SCCPU_IF_ID__H
