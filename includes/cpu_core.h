//
// Created by wenshen on 2026/1/21.
//

#ifndef SCCPU_CPU_CORE_H
#define SCCPU_CPU_CORE_H
#include "stdio.h"
#include "reg.h"
#include "pc.h"
#include "if_id.h"
#include "id_ex.h"
#include "ex_mem.h"
#include "mem_wb.h"
#include "dm.h"
#include "im.h"
#include "wb.h"
#include "utils.h"

typedef struct cpu_core {
    // ----Register state ---
    Pc32_ pc; // PC
    If_id_regs if_id;
    Id_ex_regs id_ex;
    Ex_mem_regs ex_mem;
    Mem_wb_regs mem_wb;

    // General-purpose register
    Reg324file_ rf;

    // Storage
    Dm_ dm;
    Im_t im;

    // Wires / Glue Logic
    pc_ops wire_pc_src;
    word wire_branch_target;

    //Hazard -> IF/ID/EX
    If_id_write wire_if_id_ctrl;
    Id_ex_write wire_id_ex_ctrl;

    // for dump log
    Forwarding_unit_writes forwarding_unit_writes;

    uint64_t cycle_count;
} Cpu_core;

static inline
void cpu_dump(const Cpu_core *c);

static inline
void hazard_unit_evaluate(Cpu_core *c);

static inline
void forwarding_unit(const Id_ex_regs *id_ex_regs,
                     const Ex_mem_regs *ex_mem_regs,
                     const Mem_wb_regs *mem_wb_regs,
                     Forwarding_unit_writes *fuw);

static inline
void init_cpu_c(Cpu_core *c) {
    init_pc32(&c->pc);
    init_if_id_regs(&c->if_id);
    init_id_ex_regs(&c->id_ex);
    init_ex_mem_regs(&c->ex_mem);
    init_mem_wb_regs(&c->mem_wb);
    init_reg32file(&c->rf);
    init_dm_(&c->dm);
    init_imt(&c->im);
    memset(&c->wire_pc_src, 0, sizeof(pc_ops));
    memset(&c->wire_if_id_ctrl, 0, sizeof(If_id_write));
    memset(&c->wire_id_ex_ctrl, 0, sizeof(Id_ex_write));
    c->cycle_count = 0;
}

static inline void hazard_unit_evaluate(Cpu_core *c) {
    // 1. 获取 EX 阶段算出的跳转信号 (Wire)
    const bit branch_taken = AND(c->wire_pc_src[0], NOT(c->wire_pc_src[1]));
    // 2. 生成控制信号 (Glue Logic)
    // 目前策略：遇到跳转就 Flush IF和ID
    c->wire_if_id_ctrl.if_id_flush = branch_taken;
    c->wire_id_ex_ctrl.id_ex_flush = branch_taken;

    c->wire_if_id_ctrl.pc_write = 1;
    c->wire_if_id_ctrl.if_id_write = 1;
    c->wire_id_ex_ctrl.id_ex_write = 1;
}


static inline
void cpu_tick(Cpu_core *c) {
    bit overflow_ = 0;
    // ============================================================
    // Phase 0: Combinational Logic Evaluation (CLK = 0)
    // 目标：计算所有 Wires，准备好 D 端的输入,采样(Sampling)
    // 顺序: 为了让“回环”生效，必须先算后端，再算前端
    // ============================================================
    wb_step(&c->mem_wb, &c->rf, 0);

    // 2. MEM
    bit mem_we_mask[4] = {1, 1, 1, 1};
    mem_wb_regs_step(&c->ex_mem, &c->mem_wb, &c->dm, mem_we_mask, 0);
    Forwarding_unit_writes fuw = {0};
    forwarding_unit(&c->id_ex, &c->ex_mem, &c->mem_wb, &fuw);
    c->forwarding_unit_writes = fuw;
    // ex_flush暂无
    ex_mem_regs_step(&c->id_ex, &c->ex_mem, &fuw, c->wire_pc_src, c->wire_branch_target, 0, &overflow_, 0);
    hazard_unit_evaluate(c);
    id_ex_regs_step(&c->id_ex, &c->if_id, &c->rf, &c->wire_id_ex_ctrl, 0);
    If_id_pc_ops if_ops_in;
    if_ops_in.pc_ops_[0] = c->wire_pc_src[0];
    if_ops_in.pc_ops_[1] = c->wire_pc_src[1];
    connect(c->wire_branch_target, if_ops_in.branch_target_wire);
    // todo ... jump/exception targets ...
    if_id_regs_step(&c->if_id, &c->im, &c->pc,
                    &if_ops_in, &c->wire_if_id_ctrl,
                    &overflow_, 0);


    // ============================================================
    // Phase 0: Combinational Logic Evaluation (CLK = 1)
    // 目标：Store
    // ============================================================

    // 1. WB (写回 RegFile)
    wb_step(&c->mem_wb, &c->rf, 1);

    // 2. MEM (写 Memory, 更新 MEM/WB)
    mem_wb_regs_step(&c->ex_mem, &c->mem_wb, &c->dm, mem_we_mask, 1);

    // 3. EX (更新 EX/MEM)
    ex_mem_regs_step(&c->id_ex, &c->ex_mem, &fuw, c->wire_pc_src, c->wire_branch_target, 0, &overflow_, 1);

    // 4. ID (更新 ID/EX)
    id_ex_regs_step(&c->id_ex, &c->if_id, &c->rf, &c->wire_id_ex_ctrl, 1);

    // 5. IF (更新 IF/ID 和 PC)
    if_id_regs_step(&c->if_id, &c->im, &c->pc, &if_ops_in, &c->wire_if_id_ctrl, &overflow_, 1);

    c->cycle_count++;

    // Dump Log
    cpu_dump(c);
}


/**
 * Forwarding Unit 是 EX 的回线
 * 它主要解决 指令依赖关系导致的流水线停顿问题
 * Example：
 * Instruction A -> ADD R0 R1 R2  : RO = R1 + R2
 * Instruction B -> ADD R3 RO R3  : R3 = R0 + R3
 * 因为存在流水线延迟的问题
 * A 执行完之后EX  B 在 ID
 * 当 B 进入 EX时 A 在 MEM, 关键是此时 R0 并没有 WB 写入新的值更新R0
 * 那么此时 B在EX 依赖的 RO 值是旧的值
 * 如果是 背靠背 (Back-to-Back) 的指令（A 接着 B）：
 *    延迟是 1 个周期（A 在 MEM，B 在 EX）。
 *    Forwarding 需要从 EX/MEM 拉线。
 * 如果是 隔一条指令 (A, X, B)：
 *    延迟是 2 个周期（A 在 WB，B 在 EX）。
 *    Forwarding 需要从 MEM/WB 拉线。
 * 如果是 隔两条指令 (A, X, Y, B)：
 *    延迟是 3 个周期。此时 A 已经 WB 完成了，B 在 ID 阶段就能读到新值。不需要 Forwarding，正常读就行。
 * 这个流水线延迟值是 n + 1
 *
 * Forwarding Unit 会"劫持" A 的 R0 的最新的计算结构 回线到 EX的输入(需要被选择)
 *
 *
 *
 */
static inline
void forwarding_unit(const Id_ex_regs *id_ex_regs,
                     const Ex_mem_regs *ex_mem_regs,
                     const Mem_wb_regs *mem_wb_regs,
                     Forwarding_unit_writes *fuw) {
    // 电路的本质是选择器, 取出所有可能, 选择目标

    // 我们来解决下面几种情况
    //  Instruction A -> ADD R0 R1 R2  : RO = R1 + R2
    //  1.R1 来至  EX | MEM
    //  2.R2 来至  EX | MEM
    //  3.混合 R1 来至 EX, R2 来至 MEM |  R2 来至 EX, R1 来至 MEM

    // Step 1
    // 从 id_ex_regs 中 取出待进入 ex_mem 的寄存器信息
    // Forwarding 并不关注当前这条指令 目的地是否是寄存器 而是关注上条指令目的地是否为寄存器
    // 它的目的是"劫持" input0和 input1 是否和上条指令存在输入依赖
    // 检查 RT 的输入是寄存器 而非立即值  RT = Imm = 1
    const bit exists_rt = NOT(GET_ALU_SRC_OF_SIGNALS(&id_ex_regs->decode_signals));
    //
    word rs_idx = {0}, rt_idx = {0};
    read_reg32(&id_ex_regs->rs_idx, rs_idx);
    read_reg32(&id_ex_regs->rt_idx, rt_idx);

    // 我们按距离的远近检查forwarding 条件是否成立 优先级低的放在前面
    // MEM
    // 1: 目的地必须是寄存器
    const bit mem_reg_write = GET_BIT_OF_REG32(&mem_wb_regs->wb_single, 31);
    const bit mem_to_reg = GET_BIT_OF_REG32(&mem_wb_regs->wb_single, 30);
    // 读取出目的地寄存器
    word mem_rd_idx = {0};
    word mem_final_result = {0};
    read_reg32(&mem_wb_regs->write_reg_idx, mem_rd_idx);

    word mem_alu_result = {0};
    word mem_read_data = {0};
    read_reg32(&mem_wb_regs->alu_result, mem_alu_result);
    read_reg32(&mem_wb_regs->mem_read_data, mem_read_data);
    word_mux_2_1(mem_alu_result, mem_read_data, mem_to_reg, mem_final_result);
    const bit mem_rs_eq = AND(word_eq(rs_idx, mem_rd_idx), mem_reg_write);
    const bit mem_rt_eq = AND(AND(word_eq(rt_idx, mem_rd_idx), exists_rt), mem_reg_write);

    // EX
    const bit ex_reg_write = GET_BIT_OF_REG32(&ex_mem_regs->wb_single, 31);
    word ex_rd_idx = {0};
    word ex_alu_result = {0};
    read_reg32(&ex_mem_regs->write_reg_idx, ex_rd_idx);
    read_reg32(&ex_mem_regs->alu_result, ex_alu_result);
    const bit ex_rs_eq = AND(word_eq(rs_idx, ex_rd_idx), ex_reg_write);
    const bit ex_rt_eq = AND(AND(word_eq(rt_idx, ex_rd_idx), exists_rt), ex_reg_write);

    //rs
    fuw->i0_enable = OR(ex_rs_eq, mem_rs_eq);
    word_mux_2_1(mem_final_result, ex_alu_result, ex_rs_eq, fuw->input0_forwarding_write);

    //
    fuw->i1_enable = OR(ex_rt_eq, mem_rt_eq);
    word_mux_2_1(mem_final_result, ex_alu_result, ex_rt_eq, fuw->input1_forwarding_write);
}


static inline
void cpu_dump(const Cpu_core *c) {
    printf(
        "\n================================================CpuCoreDump================================================\n");
    printf("\n=======Cycle %lu=======\n", c->cycle_count);
    // IF
    printf("[IF] PC:0x%08X | Instr:0x%08X \n", reg32_read_u32_(&c->pc.reg32), reg32_read_u32_(&c->if_id.instr));
    const uint32_t signals = reg32_read_u32_(&c->id_ex.decode_signals);
    char buffer[1024];
    dis_asm(signals, buffer);
    // ID
    printf(
        "[ID] Signals:%s\n"
        "     ReadData1:0x%08X, ReadData2:0x%08X, ImmExt:0x%08X, RsIdx:0x%04X, RtIdx:0x%04X, RdIdx:0x%04X\n",
        buffer,
        reg32_read_u32_(&c->id_ex.read_data1),
        reg32_read_u32_(&c->id_ex.read_data2),
        reg32_read_u32_(&c->id_ex.imm_ext),
        reg32_read_u32_(&c->id_ex.rs_idx),
        reg32_read_u32_(&c->id_ex.rt_idx),
        reg32_read_u32_(&c->id_ex.rd_idx)
    );
    // EX
    printf("[ForwardingUnit] Input0ForwardingWrite:0x%08X, I0Enable:%d, Input1ForwardingWrite:0x%08X, I1Enable:%d\n",
       word_to_u32(c->forwarding_unit_writes.input0_forwarding_write),
       c->forwarding_unit_writes.i0_enable,
       word_to_u32(c->forwarding_unit_writes.input1_forwarding_write),
       c->forwarding_unit_writes.i1_enable);

    printf(
        "[EX] MemSingle-Read:%d, MemSingle-Write:%d, WbSingle-RegWrite:%d, WbSingle-DataSrcToReg:%d, AluResult:0x%08X, WriteData:0x%08X, WriteRegIdx:%d%d\n",
        GET_BIT_OF_REG32(&c->ex_mem.mem_single, 31),
        GET_BIT_OF_REG32(&c->ex_mem.mem_single, 30),
        GET_BIT_OF_REG32(&c->ex_mem.wb_single, 31),
        GET_BIT_OF_REG32(&c->ex_mem.wb_single, 30),
        reg32_read_u32_(&c->ex_mem.alu_result),
        reg32_read_u32_(&c->ex_mem.write_data),
        GET_BIT_OF_REG32(&c->ex_mem.write_reg_idx, 0),
        GET_BIT_OF_REG32(&c->ex_mem.write_reg_idx, 1)
    );

    // Hazard
    printf("[Hazard]:\n");
    printf("                                Wire-if-id-ctrl: Pc-Write:%d, If-id-write:%d, If-id-flush:%d\n",
           c->wire_if_id_ctrl.pc_write, c->wire_if_id_ctrl.if_id_write, c->wire_if_id_ctrl.if_id_flush);
    printf("                                Wire-id-ex-ctrl: Id-ex-write:%d, Id-ex-flush:%d\n",
           c->wire_id_ex_ctrl.id_ex_write, c->wire_id_ex_ctrl.id_ex_flush);

    // MEM
    printf(
        "[MEM] WbSingle-RegWrite:%d, WbSingle-DataSrcToReg:%d, Mem-Read-Data:0x%08X, AluResult:0x%08X, WriteRegIdx:%d%d\n",
        GET_BIT_OF_REG32(&c->mem_wb.wb_single, 31),
        GET_BIT_OF_REG32(&c->mem_wb.wb_single, 30),
        reg32_read_u32_(&c->mem_wb.mem_read_data),
        reg32_read_u32_(&c->mem_wb.alu_result),
        GET_BIT_OF_REG32(&c->mem_wb.write_reg_idx, 0),
        GET_BIT_OF_REG32(&c->mem_wb.write_reg_idx, 1)
    );

    // General-purpose register
    printf("[General-Purpose-Register Dump]:\n");
    printf("                                R0:0x%08X\n", reg32_read_u32_(&c->rf.r0));
    printf("                                R1:0x%08X\n", reg32_read_u32_(&c->rf.r1));
    printf("                                R2:0x%08X\n", reg32_read_u32_(&c->rf.r2));
    printf("                                R3:0x%08X\n", reg32_read_u32_(&c->rf.r3));

    // Wires / Glue Logic
    printf("[Wires/Glue-Logic]:\n");
    printf("                                Pc-ops:%d%d\n", c->wire_pc_src[0], c->wire_pc_src[1]);
    printf("                                Wire_branch_target:0x%08X\n", u32_from_word(c->wire_branch_target));

}


#endif //SCCPU_CPU_CORE_H
