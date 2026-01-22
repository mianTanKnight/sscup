//
// Created by wenshen on 2026/1/21.
//

#ifndef SCCPU_CPU_CORE_H
#define SCCPU_CPU_CORE_H
#include "stdio.h"
#include "reg_.h"
#include "pc_.h"
#include "if_id_.h"
#include "id_ex_.h"
#include "ex_mem.h"
#include "mem_wb.h"
#include "dm_.h"
#include "im_.h"
#include "wb_.h"
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
    const uint32_t pc = reg32_read_u32_(&c->pc.reg32);
    const uint32_t instr = reg32_read_u32_(&c->if_id.instr);
    printf("[IF] PC:0x%08X | Instr:0x%08X \n", pc, instr);
    const uint32_t signals = reg32_read_u32_(&c->id_ex.decode_signals);
    char buffer[1024];
    dis_asm(signals, buffer);
    printf(
        "[EX] Signals:%s\n"
        "     ReadData1:0x%08X, ReadData2:0x%08X, ImmExt:0x%08X, RsIdx:0x%04X, RtIdx:0x%04X, RdIdx:0x%04X\n",
        buffer,
        reg32_read_u32_(&c->id_ex.read_data1),
        reg32_read_u32_(&c->id_ex.read_data2),
        reg32_read_u32_(&c->id_ex.imm_ext),
        reg32_read_u32_(&c->id_ex.rs_idx),
        reg32_read_u32_(&c->id_ex.rt_idx),
        reg32_read_u32_(&c->id_ex.rd_idx)
    );
}


#endif //SCCPU_CPU_CORE_H
