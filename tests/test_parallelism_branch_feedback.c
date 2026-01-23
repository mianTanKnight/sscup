//
// Created by wenshen on 2026/1/9.
//

#include "common_test.h"
#include "../includes/utils.h"

#ifdef ASSERT_EQ_U32
#undef ASSERT_EQ_U32
#endif
#ifdef ASSERT_EQ_BIT
#undef ASSERT_EQ_BIT
#endif

#define ASSERT_EQ_U32(name, actual, expected) do { \
    uint32_t a__ = (uint32_t)(actual); \
    uint32_t e__ = (uint32_t)(expected); \
    if (a__ != e__) { \
        printf("[FAIL] %s: got=0x%08X (%u), expected=0x%08X (%u)\n", \
               (name), (unsigned)a__, (unsigned)a__, (unsigned)e__, (unsigned)e__); \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
    } \
} while (0)

#define ASSERT_EQ_BIT(name, actual, expected) do { \
    int a__ = ((actual) & 1); \
    int e__ = ((expected) & 1); \
    if (a__ != e__) { \
        printf("[FAIL] %s: got=%d, expected=%d\n", (name), a__, e__); \
        return 1; \
    } else { \
        printf("[PASS] %s\n", (name)); \
    } \
} while (0)

// ------------------------------------------------------------
// 一个“cpu 单相位调用”：在某个 clk(0/1) 下让各模块各走一步。
// 关键点：
//   - EX 先算出 pc_src/branch_target（wire）
//   - hazard 把 wire 立刻驱动到 IF 端 ops（wire bundle）
//   - IF 再使用 ops 选择 pc_next_wire，并在 clk=1 上沿更新 PC/IFID
//   - ID 只从 IF/ID.Q 读（上一拍的东西），并锁存到 ID/EX
//
// 注意：这段顺序不是“硬件有顺序”，而是 C 里模拟组合逻辑稳定的求值顺序。
// ------------------------------------------------------------
typedef struct cpu_t {
    Pc32_ pc;
    If_id_regs ifid;
    Id_ex_regs idex;
    Ex_mem_regs exmem;

    If_id_pc_ops pc_ops; // wire bundle: pc_src + branch_target
    If_id_write ifid_write; // pc_write/if_id_write/if_id_flush
    Id_ex_write id_ex_write;

    Reg324file_ rf;
} Cpu_t;


static void print_pipeline_state(const char *tag, Cpu_t *cpu) {
    uint32_t pc_v, ifid_instr_v, ifid_pc4_v;
    uint32_t idex_rd1_v, idex_rd2_v, idex_rs_idx_v, idex_rt_idx_v;
    uint32_t exmem_alu_v;

    // IF/ID Stage
    read_pc_u32(&cpu->pc, &pc_v);
    read_ifid_instr_u32(&cpu->ifid, &ifid_instr_v);
    read_ifid_pc4_u32(&cpu->ifid, &ifid_pc4_v);

    // ID/EX Stage
    idex_rd1_v = reg32_read_u32(&cpu->idex.read_data1);
    idex_rd2_v = reg32_read_u32(&cpu->idex.read_data2);
    idex_rs_idx_v = reg32_read_u32(&cpu->idex.rs_idx) & 3;
    idex_rt_idx_v = reg32_read_u32(&cpu->idex.rt_idx) & 3;

    // EX/MEM Stage
    exmem_alu_v = reg32_read_u32(&cpu->exmem.alu_result);

    printf("--- %s ---\n", tag);
    printf("  [IF]  PC = 0x%08X\n", pc_v);
    printf("  [IF/ID] instr = 0x%08X, pc+4 = 0x%08X\n", ifid_instr_v, ifid_pc4_v);
    printf("  [ID/EX] RData1=0x%X, RData2=0x%X (rs=%u,rt=%u)\n", idex_rd1_v, idex_rd2_v, idex_rs_idx_v, idex_rt_idx_v);
    printf("  [EX/MEM] ALU_res = 0x%X\n", exmem_alu_v);
    printf("----------------------------------\n");
}


static inline
void init_cpu(Cpu_t *c) {
    init_pc32(&c->pc);
    init_if_id_regs(&c->ifid);
    init_id_ex_regs(&c->idex);
    init_ex_eme_regs(&c->exmem);
    init_reg32file(&c->rf);
    memset(&c->pc_ops, 0, sizeof(If_id_pc_ops));
    memset(&c->ifid_write, 0, sizeof(If_id_write));
    memset(&c->id_ex_write, 0, sizeof(Id_ex_write));
}

static inline void hazard_comb_c(Cpu_t *c, const pc_ops pc_src, const word branch_target) {
    c->pc_ops.pc_ops_[0] = pc_src[0];
    c->pc_ops.pc_ops_[1] = pc_src[1];
    // 把 EX 的 pc_src/branch_target wire “立即”连接到 IF 的 pc_ops
    // connect wire if_id ops line
    for (int i = 0; i < WORD_SIZE; ++i) {
        c->pc_ops.branch_target_wire[i] = branch_target[i];
    }
    // c->ifid_write 看起来是被"储存"了
    // 因为 branch_taken 是被实时计算出来的 也就是说 在 step2 阶段 branch_taken 也是会被计算的
    // c->ifid_write.if_id_flush = branch_taken; 更像是连接
    bit branch_taken = AND(pc_src[0], NOT(pc_src[1]));
    c->ifid_write.if_id_flush = branch_taken;
    c->id_ex_write.id_ex_flush = branch_taken;
}


static inline void cpu_phase(Cpu_t *c, const Im_t *im, bit clk) {
    bit overflow = 0;
    // --- Wires for feedback ---
    pc_ops pc_src = {0, 0};
    word branch_target = {0};

    // --- Hazard Control Wires ---
    bit pc_write = 1;
    bit if_id_write = 1;
    bit id_ex_write = 1;
    // init
    c->pc_ops.pc_ops_[0] = 0;
    c->pc_ops.pc_ops_[1] = 0;
    c->ifid_write.if_id_flush = 0;
    c->id_ex_write.id_ex_flush = 0;

    // Reads from ID/EX.Q
    ex_mem_regs_step(&c->idex, &c->exmem, pc_src, branch_target, 0, &overflow, clk);
    // hazard
    hazard_comb_c(c, pc_src, branch_target);

    c->id_ex_write.id_ex_write = id_ex_write;
    // // Reads from IF/ID.Q
    id_ex_regs_step(&c->idex, &c->ifid, &c->rf, &c->id_ex_write, clk);

    c->ifid_write.pc_write = pc_write;
    c->ifid_write.if_id_write = if_id_write;
    if_id_regs_step(&c->ifid, im, &c->pc, &c->pc_ops, &c->ifid_write, &overflow, clk);
}

static inline void cpu_cycle(Cpu_t *c, const Im_t *im) {
    printf("  >>> CLK=0: Combinational Logic Evaluation <<<\n");
    cpu_phase(c, im, 0);
    printf("\n debug2 ->%X \n", reg32_read_u32(&c->pc.reg32));
    printf("  >>> CLK=1: Sequential Logic (Register) Update <<<\n");
    cpu_phase(c, im, 1);
}

// ------------------------------------------------------------
// 核心演示测试：BEQ 在 EX 决策，IF 会在同拍取到顺序错误指令，但会被 flush
//
// 程序布局：
//   PC=0   : BEQ R1, R1, +2    -> target = PC+4 + (2<<2) = 12
//   PC=4   : A（无所谓是什么）
//   PC=8   : WRONG（将被 flush）
//   PC=12  : TARGET（最终应该被取到）
//
// 期望：
// cycle0 end: PC=4,  IF/ID=BEQ
// cycle1 end: PC=8,  IF/ID=A,     ID/EX=BEQ
// cycle2 end: PC=12, IF/ID=NOP(0) （因为 EX 决定 branch，flush 掉 PC=8 那条）
// cycle3 end: PC=16, IF/ID=TARGET
// ------------------------------------------------------------
static int test_parallelism_branch_feedback(void) {
    printf("=== test_parallelism_branch_feedback ===\n");

    Cpu_t cpu;
    init_cpu(&cpu);

    Im_t im;
    init_imt(&im);

    // Set 123 For R1
    reg32_write_u32(&cpu.rf.r1, 123);

    const uint32_t I_BEQ = enc_beq(1, 1, 2);
    const uint32_t I_A = 0xAAAAAAAA;
    const uint32_t I_WRONG = 0xBBBBBBBB;
    const uint32_t I_TGT = enc_addi(2, 2, 1);

    // Set test inc For Im
    im_set_u32(&im, 0, I_BEQ);
    im_set_u32(&im, 1, I_A);
    im_set_u32(&im, 2, I_WRONG);
    im_set_u32(&im, 3, I_TGT);

    // Init 0 For Pc
    reg32_write_u32(&cpu.pc.reg32, 0);

    // === Cycle 0: Fetch BEQ ===
    printf("\n>>> Cycle 0 Start <<<\n");
    print_pipeline_state("Initial State", &cpu);
    cpu_cycle(&cpu, &im);
    print_pipeline_state("End of Cycle 0", &cpu);
    ASSERT_EQ_U32("cycle0 PC==4", reg32_read_u32(&cpu.pc.reg32), 4);
    ASSERT_EQ_U32("cycle0 IF/ID.instr==BEQ", reg32_read_u32(&cpu.ifid.instr), I_BEQ);
    printf("--- Cycle 0 OK ---\n");

    // === Cycle 1: Fetch Instr A, Decode BEQ ===
    printf("\n>>> Cycle 1 Start <<<\n");
    printf("\n debug1 ->%X \n", reg32_read_u32(&cpu.pc.reg32));
    cpu_cycle(&cpu, &im);
    print_pipeline_state("End of Cycle 1", &cpu);
    ASSERT_EQ_U32("cycle1 PC==8", reg32_read_u32(&cpu.pc.reg32), 8); // <<-- 期望是 8！
    ASSERT_EQ_U32("cycle1 IF/ID.instr==A", reg32_read_u32(&cpu.ifid.instr), I_A);
    printf("--- Cycle 1 OK ---\n");

    // === Cycle 2: Fetch WRONG, Decode A, Execute BEQ ===
    printf("\n>>> Cycle 2 Start (Branch Decision Happens here!) <<<\n");
    cpu_cycle(&cpu, &im);
    print_pipeline_state("End of Cycle 2", &cpu);
    ASSERT_EQ_U32("cycle2 PC==12 (branch target)", reg32_read_u32((Reg32_*)&cpu.pc.reg32), 12);
    ASSERT_EQ_U32("cycle2 IF/ID.instr == NOP", reg32_read_u32(&cpu.ifid.instr), 0);
    printf("--- Cycle 2 OK ---\n");

    return 0;
}

// int main(void) {
//     int rc = 0;
//     rc |= test_parallelism_branch_feedback();
//     if (rc == 0) {
//         printf("ALL PARALLELISM TESTS PASSED ✅\n");
//     }
//     return rc;
// }
