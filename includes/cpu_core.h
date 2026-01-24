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

    uint64_t cycle_count;
} Cpu_core;

static inline
void cpu_dump(const Cpu_core *c);

static inline
void hazard_unit_evaluate(Cpu_core *c);

static inline
void init_cpu_c(Cpu_core *c) {
    init_pc32(&c->pc);
    init_if_id_regs(&c->if_id);
    init_id_ex_regs(&c->id_ex);
    init_ex_eme_regs(&c->ex_mem);
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

    // 2. MEM 阶段
    //write_enabled 掩码暂时全1
    bit mem_we_mask[4] = {1, 1, 1, 1};
    mem_wb_regs_step(&c->ex_mem, &c->mem_wb, &c->dm, mem_we_mask, 0);

    // ex_flush暂无
    ex_mem_regs_step(&c->id_ex, &c->ex_mem, c->wire_pc_src, c->wire_branch_target,
                     0, &overflow_, 0);
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
    ex_mem_regs_step(&c->id_ex, &c->ex_mem, c->wire_pc_src, c->wire_branch_target, 0, &overflow_, 1);

    // 4. ID (更新 ID/EX)
    id_ex_regs_step(&c->id_ex, &c->if_id, &c->rf, &c->wire_id_ex_ctrl, 1);

    // 5. IF (更新 IF/ID 和 PC)
    if_id_regs_step(&c->if_id, &c->im, &c->pc, &if_ops_in, &c->wire_if_id_ctrl, &overflow_, 1);

    c->cycle_count++;

    // Dump Log
    cpu_dump(c);
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
    // MEM
    printf(
        "[MEM] WbSingle-RegWrite:%d, WbSingle-DataSrcToReg:%d, Mem-Read-Data:0x%08X, AluResult:0x%08X, WriteRegIdx:%d%d\n",
        GET_BIT_OF_REG32(&c->mem_wb.wb_single, 31),
        GET_BIT_OF_REG32(&c->mem_wb.wb_single, 30),
        reg32_read_u32_(&c->mem_wb.mem_read_data),
        reg32_read_u32_(&c->mem_wb.alu_result),
        GET_BIT_OF_REG32(&c->ex_mem.write_reg_idx, 0),
        GET_BIT_OF_REG32(&c->ex_mem.write_reg_idx, 1)
    );

    // General-purpose register
    printf("[General-Purpose-Register Dump]:\n");
    printf("                                RO:0x%08X\n", reg32_read_u32_(&c->rf.r0));
    printf("                                R1:0x%08X\n", reg32_read_u32_(&c->rf.r1));
    printf("                                R2:0x%08X\n", reg32_read_u32_(&c->rf.r2));
    printf("                                R3:0x%08X\n", reg32_read_u32_(&c->rf.r3));

    // Wires / Glue Logic
    printf("[Wires/Glue-Logic]:\n");
    printf("                                Pc-ops:%d%d\n", c->wire_pc_src[0], c->wire_pc_src[1]);
    printf("                                Wire_branch_target:0x%08X\n", u32_from_word(c->wire_branch_target));

    // Hazard
    printf("[Hazard]:\n");
    printf("                                Wire-if-id-ctrl: Pc-Write:%d, If-id-write:%d, If-id-flush:%d\n",
           c->wire_if_id_ctrl.pc_write, c->wire_if_id_ctrl.if_id_write, c->wire_if_id_ctrl.if_id_flush);
    printf("                                Wire-id-ex-ctrl: Id-ex-write:%d, Id-ex-flush:%d\n",
           c->wire_id_ex_ctrl.id_ex_write, c->wire_id_ex_ctrl.id_ex_flush);
}


#endif //SCCPU_CPU_CORE_H
