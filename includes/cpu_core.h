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
