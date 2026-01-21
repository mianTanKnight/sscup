// test_ex_mem.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../includes/common_.h"
#include "../includes/reg_.h"
#include "../includes/if_id_.h"
#include "../includes/id_ex_.h"
#include "../includes/ex_mem.h"
#include "../includes/common_test.h"
// -------------------------
// 你在 ID/EX 里对 decode_signals 的布局：
//  bit31..bit21 使用（高位开始），其余为 0
// 31 reg_dst
// 30 alu_src
// 29 data_src_to_reg
// 28 reg_write
// 27 mem_read
// 26 mem_write
// 25 branch
// 24 jump
// 23 ops0
// 22 ops1
// 21 ops2
// -------------------------
static inline void pack_decode_signals_word(word out,
                                            bit reg_dst, bit alu_src, bit data_src_to_reg,
                                            bit reg_write, bit mem_read, bit mem_write,
                                            bit branch, bit jump,
                                            const ops alu_ops) {
    zero_word(out);
    out[INST_WORD(31)] = reg_dst;
    out[INST_WORD(30)] = alu_src;
    out[INST_WORD(29)] = data_src_to_reg;
    out[INST_WORD(28)] = reg_write;
    out[INST_WORD(27)] = mem_read;
    out[INST_WORD(26)] = mem_write;
    out[INST_WORD(25)] = branch;
    out[INST_WORD(24)] = jump;
    out[INST_WORD(23)] = alu_ops[0];
    out[INST_WORD(22)] = alu_ops[1];
    out[INST_WORD(21)] = alu_ops[2];
}

static inline void idex_load_minimal(Id_ex_regs *idex,
                                     uint32_t decode_bits_u32,
                                     uint32_t rdata1, uint32_t rdata2,
                                     uint32_t imm_ext,
                                     uint32_t rs_idx, uint32_t rt_idx, uint32_t rd_idx,
                                     uint32_t pc_plus4) {
    // decode_signals 是 reg32_，直接写入对应的 32bit 图样
    reg32_write_u32(&idex->decode_signals, decode_bits_u32);
    reg32_write_u32(&idex->read_data1, rdata1);
    reg32_write_u32(&idex->read_data2, rdata2);
    reg32_write_u32(&idex->imm_ext, imm_ext);
    reg32_write_u32(&idex->rs_idx, rs_idx);
    reg32_write_u32(&idex->rt_idx, rt_idx);
    reg32_write_u32(&idex->rd_idx, rd_idx);
    reg32_write_u32(&idex->pc_plus4, pc_plus4);
}

// 把 word decode_signals 转成 u32（方便写入 reg32）
static inline uint32_t word_to_u32(const word w) {
    return u32_from_word_local(w);
}

// 二段式调用 EX/MEM step（模拟你项目一周期两次调用）
static inline void exmem_tick(const Id_ex_regs *idex, Ex_mem_regs *exmem,
                              pc_ops pc_src, word branch_target,
                              bit ex_flush, bit *overflow) {
    ex_mem_regs_step(idex, exmem, pc_src, branch_target, ex_flush, overflow, 0);
    ex_mem_regs_step(idex, exmem, pc_src, branch_target, ex_flush, overflow, 1);
}

// -------------------------
// TEST 1: R-Type ADD，alu_src=0 选 RT，reg_dst=1 选 RD
// -------------------------
static int test_exmem_rtype_add_basic(void) {
    printf("=== test_exmem_rtype_add_basic ===\n");

    Id_ex_regs idex;
    Ex_mem_regs exmem;


    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    init_reg32(&exmem.mem_single);
    init_reg32(&exmem.wb_single);
    init_reg32(&exmem.alu_result);
    init_reg32(&exmem.write_data);
    init_reg32(&exmem.write_reg_idx);


    word sigw;
    pack_decode_signals_word(sigw,
                             /*reg_dst*/1, /*alu_src*/0, /*mem_to_reg*/0,
                             /*reg_write*/1, /*mem_read*/0, /*mem_write*/0,
                             /*branch*/0, /*jump*/0,
                             OPS_ADD_);
    uint32_t sig_u32 = word_to_u32(sigw);

    // rdata1=5, rdata2=7, imm=123(不该被用到)
    // rt_idx=3, rd_idx=1 => reg_dst=1 => write_reg_idx 应该是 1
    idex_load_minimal(&idex, sig_u32,
                      5, 7, 123,
                      2, 3, 1,
                      100);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    // 注意：pc_src/branch_target 是导线输出，不需要等上沿
    ex_mem_regs_step(&idex, &exmem, pc_src, branch_target, /*ex_flush*/0, &ov, 0);

    ASSERT_EQ_BIT("pc_src[0] == 0 (no branch)", pc_src[0], 0);
    ASSERT_EQ_BIT("pc_src[1] == 0", pc_src[1], 0);

    // 提交锁存
    ex_mem_regs_step(&idex, &exmem, pc_src, branch_target, /*ex_flush*/0, &ov, 1);

    uint32_t alu_res = reg32_read_u32(&exmem.alu_result);
    uint32_t wdata = reg32_read_u32(&exmem.write_data);
    uint32_t widx = reg32_read_u32(&exmem.write_reg_idx);

    ASSERT_EQ_U32("alu_result == 12", alu_res, 12);
    ASSERT_EQ_U32("write_data == RT(7)", wdata, 7);
    ASSERT_EQ_U32("write_reg_idx == RD(1)", widx, 1);

    return 0;
}

// -------------------------
// TEST 2: I-Type ADDI，alu_src=1 选 imm，reg_dst=0 选 RT
// -------------------------
static int test_exmem_itype_addi_imm_mux(void) {
    printf("=== test_exmem_itype_addi_imm_mux ===\n");

    Id_ex_regs idex;
    Ex_mem_regs exmem;
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    init_reg32(&exmem.mem_single);
    init_reg32(&exmem.wb_single);
    init_reg32(&exmem.alu_result);
    init_reg32(&exmem.write_data);
    init_reg32(&exmem.write_reg_idx);


    word sigw;
    pack_decode_signals_word(sigw,
                             /*reg_dst*/0, /*alu_src*/1, /*mem_to_reg*/0,
                             /*reg_write*/1, /*mem_read*/0, /*mem_write*/0,
                             /*branch*/0, /*jump*/0,
                             OPS_ADD_);
    uint32_t sig_u32 = word_to_u32(sigw);

    // rdata1=1000, rdata2=777(不该被用到), imm=9 => result=1009
    // rt_idx=2, rd_idx=1 => reg_dst=0 => write_reg_idx 应该是 RT=2
    idex_load_minimal(&idex, sig_u32,
                      1000, 777, 9,
                      0, 2, 1,
                      200);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    exmem_tick(&idex, &exmem, pc_src, branch_target, /*ex_flush*/0, &ov);

    ASSERT_EQ_U32("alu_result == 1009", reg32_read_u32(&exmem.alu_result), 1009);
    ASSERT_EQ_U32("write_reg_idx == RT(2)", reg32_read_u32(&exmem.write_reg_idx), 2);

    return 0;
}

// -------------------------
// TEST 3: BEQ taken：branch=1 且 read_data1==read_data2
// 期望：pc_src=BRANCH_TARGET(10)->pc_src[0]=1, pc_src[1]=0
// branch_target = pc_plus4 + (imm_ext << 2)
// -------------------------
static int test_exmem_branch_taken_pc_feedback(void) {
    printf("=== test_exmem_branch_taken_pc_feedback ===\n");

    Id_ex_regs idex;
    Ex_mem_regs exmem;
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    init_reg32(&exmem.mem_single);
    init_reg32(&exmem.wb_single);
    init_reg32(&exmem.alu_result);
    init_reg32(&exmem.write_data);
    init_reg32(&exmem.write_reg_idx);


    word sigw;
    // BEQ 你 decode 里通常让 ops=SUB，但这里 EX 自己用 SUB 做比较，不依赖 ops
    pack_decode_signals_word(sigw,
                             /*reg_dst*/0, /*alu_src*/0, /*mem_to_reg*/0,
                             /*reg_write*/0, /*mem_read*/0, /*mem_write*/0,
                             /*branch*/1, /*jump*/0,
                             OPS_SUB_);
    uint32_t sig_u32 = word_to_u32(sigw);

    // pc_plus4 = 8, imm_ext=2 => target = 8 + (2<<2)=16
    // read_data1 == read_data2 == 0x1234 => taken
    idex_load_minimal(&idex, sig_u32,
                      0x1234, 0x1234, 2,
                      0, 0, 0,
                      8);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    // 在 clk=0 就应当稳定输出 pc_src/branch_target（导线）
    ex_mem_regs_step(&idex, &exmem, pc_src, branch_target, /*ex_flush*/0, &ov, 0);

    ASSERT_EQ_BIT("pc_src[0]==1 (BRANCH_TARGET)", pc_src[0], 1);
    ASSERT_EQ_BIT("pc_src[1]==0 (BRANCH_TARGET)", pc_src[1], 0);
    ASSERT_EQ_U32("branch_target == 16", u32_from_word_local(branch_target), 16);

    // 锁存一次，确认 bubble/副作用信号不应被写（这里 reg_write=0）
    ex_mem_regs_step(&idex, &exmem, pc_src, branch_target, /*ex_flush*/0, &ov, 1);

    return 0;
}

// -------------------------
// TEST 4: BEQ not taken：branch=1 但 read_data1!=read_data2
// 期望 pc_src == 00
// -------------------------
static int test_exmem_branch_not_taken(void) {
    printf("=== test_exmem_branch_not_taken ===\n");

    Id_ex_regs idex;
    Ex_mem_regs exmem;
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    init_reg32(&exmem.mem_single);
    init_reg32(&exmem.wb_single);
    init_reg32(&exmem.alu_result);
    init_reg32(&exmem.write_data);
    init_reg32(&exmem.write_reg_idx);


    word sigw;
    pack_decode_signals_word(sigw,
                             0, 0, 0,
                             0, 0, 0,
                             1, 0,
                             OPS_SUB_);
    uint32_t sig_u32 = word_to_u32(sigw);

    idex_load_minimal(&idex, sig_u32,
                      0x1111, 0x2222, 2,
                      0, 0, 0,
                      8);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    ex_mem_regs_step(&idex, &exmem, pc_src, branch_target, /*ex_flush*/0, &ov, 0);

    ASSERT_EQ_BIT("pc_src[0]==0", pc_src[0], 0);
    ASSERT_EQ_BIT("pc_src[1]==0", pc_src[1], 0);

    return 0;
}

// -------------------------
// TEST 5: ex_flush bubble：所有 EX/MEM 锁存输出应为 0
// 且 pc_src 被抑制（NOT(ex_flush)）
// -------------------------
static int test_exmem_flush_bubble(void) {
    printf("=== test_exmem_flush_bubble ===\n");

    Id_ex_regs idex;
    Ex_mem_regs exmem;
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    init_reg32(&exmem.mem_single);
    init_reg32(&exmem.wb_single);
    init_reg32(&exmem.alu_result);
    init_reg32(&exmem.write_data);
    init_reg32(&exmem.write_reg_idx);


    // 构造一个“本来会写寄存器+可能分支”的指令，然后 flush 掉
    word sigw;
    pack_decode_signals_word(sigw,
                             /*reg_dst*/1, /*alu_src*/0, /*mem_to_reg*/0,
                             /*reg_write*/1, /*mem_read*/1, /*mem_write*/1,
                             /*branch*/1, /*jump*/0,
                             OPS_ADD_);
    uint32_t sig_u32 = word_to_u32(sigw);

    idex_load_minimal(&idex, sig_u32,
                      5, 7, 2,
                      0, 3, 1,
                      8);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    // flush=1
    exmem_tick(&idex, &exmem, pc_src, branch_target, /*ex_flush*/1, &ov);

    // 1) 分支信号应被抑制（这依赖你 ex_flush 的语义：bubble/kill 当前这条）
    ASSERT_EQ_BIT("pc_src[0]==0 when ex_flush=1", pc_src[0], 0);
    ASSERT_EQ_BIT("pc_src[1]==0", pc_src[1], 0);

    // 2) EX/MEM 锁存结果应全清零（你代码里都 mux 到 WORD_ZERO 了）
    ASSERT_EQ_U32("alu_result==0", reg32_read_u32(&exmem.alu_result), 0);
    ASSERT_EQ_U32("write_data==0", reg32_read_u32(&exmem.write_data), 0);
    ASSERT_EQ_U32("write_reg_idx==0", reg32_read_u32(&exmem.write_reg_idx), 0);

    // mem_single / wb_single 你布局可能在高位，这里做“整体为0”的强断言最安全
    ASSERT_EQ_U32("mem_single==0", reg32_read_u32(&exmem.mem_single), 0);
    ASSERT_EQ_U32("wb_single==0", reg32_read_u32(&exmem.wb_single), 0);

    return 0;
}
//
// int main_(void) {
//     int rc = 0;
//     rc |= test_exmem_rtype_add_basic();
//     rc |= test_exmem_itype_addi_imm_mux();
//     rc |= test_exmem_branch_taken_pc_feedback();
//     rc |= test_exmem_branch_not_taken();
//     rc |= test_exmem_flush_bubble();
//
//     if (rc == 0) {
//         printf("\nALL EX/MEM TESTS PASSED ✅\n");
//     }
//     return rc;
// }
