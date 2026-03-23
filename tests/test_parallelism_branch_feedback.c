// test_parallelism_branch_feedback.c
// Integration test: BEQ branch decision in EX, flush IF/ID and ID/EX

#include <stdio.h>
#include "../includes/cpu_core.h"
#include "common_test.h"

// Simplified CPU struct for this integration test (no MEM/WB/DM)
typedef struct cpu_t {
    Pc32_ pc;
    If_id_regs ifid;
    Id_ex_regs idex;
    Ex_mem_regs exmem;

    If_id_pc_ops pc_ops;
    If_id_write ifid_write;
    Id_ex_write id_ex_write;

    Reg324file_ rf;
} Cpu_t;

static inline void init_cpu(Cpu_t *c) {
    init_pc32(&c->pc);
    init_if_id_regs(&c->ifid);
    init_id_ex_regs(&c->idex);
    init_ex_mem_regs(&c->exmem);
    init_reg32file(&c->rf);
    memset(&c->pc_ops, 0, sizeof(If_id_pc_ops));
    memset(&c->ifid_write, 0, sizeof(If_id_write));
    memset(&c->id_ex_write, 0, sizeof(Id_ex_write));
}

static inline void hazard_comb_c(Cpu_t *c, const pc_ops pc_src, const word branch_target) {
    c->pc_ops.pc_ops_[0] = pc_src[0];
    c->pc_ops.pc_ops_[1] = pc_src[1];
    for (int i = 0; i < WORD_SIZE; ++i)
        c->pc_ops.branch_target_wire[i] = branch_target[i];

    bit branch_taken = AND(pc_src[0], NOT(pc_src[1]));
    c->ifid_write.if_id_flush = branch_taken;
    c->id_ex_write.id_ex_flush = branch_taken;
}

static inline void cpu_phase(Cpu_t *c, const Im_t *im, bit clk) {
    bit overflow = 0;
    pc_ops pc_src = {0, 0};
    word branch_target = {0};

    c->pc_ops.pc_ops_[0] = 0;
    c->pc_ops.pc_ops_[1] = 0;
    c->ifid_write.if_id_flush = 0;
    c->id_ex_write.id_ex_flush = 0;

    Forwarding_unit_writes fuw = {0};
    ex_mem_regs_step(&c->idex, &c->exmem, &fuw, pc_src, branch_target, 0, &overflow, clk);
    hazard_comb_c(c, pc_src, branch_target);

    c->id_ex_write.id_ex_write = 1;
    id_ex_regs_step(&c->idex, &c->ifid, &c->rf, &c->id_ex_write, clk);

    c->ifid_write.pc_write = 1;
    c->ifid_write.if_id_write = 1;
    if_id_regs_step(&c->ifid, im, &c->pc, &c->pc_ops, &c->ifid_write, &overflow, clk);
}

static inline void cpu_cycle(Cpu_t *c, const Im_t *im) {
    cpu_phase(c, im, 0);
    cpu_phase(c, im, 1);
}

// ======================== Test ========================
// Program:
//   PC=0  : BEQ R1, R1, +2    -> target = 4 + (2<<2) = 12
//   PC=4  : A (dummy)
//   PC=8  : WRONG (will be flushed)
//   PC=12 : TARGET (should be fetched)
//
// cycle0 end: PC=4,  IF/ID=BEQ
// cycle1 end: PC=8,  IF/ID=A,     ID/EX=BEQ
// cycle2 end: PC=12, IF/ID=NOP(0) (EX decides branch, flush)

static int test_parallelism_branch_feedback(void) {
    printf("=== test_parallelism_branch_feedback ===\n");

    Cpu_t cpu;
    init_cpu(&cpu);

    Im_t im;
    init_imt(&im);

    reg32_write_u32(&cpu.rf.r1, 123);

    const uint32_t I_BEQ = enc_beq(1, 1, 2);
    const uint32_t I_A   = 0xAAAAAAAA;
    const uint32_t I_WRONG = 0xBBBBBBBB;
    const uint32_t I_TGT = enc_addi(2, 2, 1);

    im_set_u32(&im, 0, I_BEQ);
    im_set_u32(&im, 1, I_A);
    im_set_u32(&im, 2, I_WRONG);
    im_set_u32(&im, 3, I_TGT);

    reg32_write_u32(&cpu.pc.reg32, 0);

    // Cycle 0: Fetch BEQ
    cpu_cycle(&cpu, &im);
    ASSERT_EQ_U32("cycle0 PC==4", reg32_read_u32(&cpu.pc.reg32), 4);
    ASSERT_EQ_U32("cycle0 IF/ID.instr==BEQ", reg32_read_u32(&cpu.ifid.instr), I_BEQ);

    // Cycle 1: Fetch A, Decode BEQ
    cpu_cycle(&cpu, &im);
    ASSERT_EQ_U32("cycle1 PC==8", reg32_read_u32(&cpu.pc.reg32), 8);
    ASSERT_EQ_U32("cycle1 IF/ID.instr==A", reg32_read_u32(&cpu.ifid.instr), I_A);

    // Cycle 2: EX decides branch, flush
    cpu_cycle(&cpu, &im);
    ASSERT_EQ_U32("cycle2 PC==12 (branch target)", reg32_read_u32(&cpu.pc.reg32), 12);
    ASSERT_EQ_U32("cycle2 IF/ID.instr == NOP", reg32_read_u32(&cpu.ifid.instr), 0);

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_parallelism_branch_feedback);
    TEST_SUMMARY("Parallelism Branch Feedback Tests");
    return rc;
}
