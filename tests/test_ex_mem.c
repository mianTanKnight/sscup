// test_ex_mem.c
// EX/MEM pipeline register tests: ALU, branch taken/not-taken, flush

#include <stdio.h>
#include "../includes/ex_mem.h"
#include "common_test.h"

// ======================== Helpers ========================

static inline void pack_decode_signals_word(word out,
                                            bit reg_dst, bit alu_src, bit data_src_to_reg,
                                            bit reg_write, bit mem_read, bit mem_write,
                                            bit branch, bit jump,
                                            const ops alu_ops) {
    memset(out, 0, sizeof(word));
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
    reg32_write_u32(&idex->decode_signals, decode_bits_u32);
    reg32_write_u32(&idex->read_data1, rdata1);
    reg32_write_u32(&idex->read_data2, rdata2);
    reg32_write_u32(&idex->imm_ext, imm_ext);
    reg32_write_u32(&idex->rs_idx, rs_idx);
    reg32_write_u32(&idex->rt_idx, rt_idx);
    reg32_write_u32(&idex->rd_idx, rd_idx);
    reg32_write_u32(&idex->pc_plus4, pc_plus4);
}

static inline void exmem_tick(const Id_ex_regs *idex, Ex_mem_regs *exmem,
                              pc_ops pc_src, word branch_target,
                              bit ex_flush, bit *overflow) {
    Forwarding_unit_writes fuw = {0};
    ex_mem_regs_step(idex, exmem, &fuw, pc_src, branch_target, ex_flush, overflow, 0);
    ex_mem_regs_step(idex, exmem, &fuw, pc_src, branch_target, ex_flush, overflow, 1);
}

// ======================== Tests ========================

static int test_exmem_rtype_add_basic(void) {
    printf("=== test_exmem_rtype_add_basic ===\n");

    Id_ex_regs idex; Ex_mem_regs exmem;
    init_id_ex_regs(&idex); init_ex_mem_regs(&exmem);

    word sigw;
    pack_decode_signals_word(sigw, 1, 0, 0, 1, 0, 0, 0, 0, OPS_ADD_);

    // rdata1=5, rdata2=7, rd_idx=1
    idex_load_minimal(&idex, word_to_u32(sigw), 5, 7, 123, 2, 3, 1, 100);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;
    Forwarding_unit_writes fuw = {0};

    ex_mem_regs_step(&idex, &exmem, &fuw, pc_src, branch_target, 0, &ov, 0);
    ASSERT_EQ_BIT("pc_src[0] == 0 (no branch)", pc_src[0], 0);
    ASSERT_EQ_BIT("pc_src[1] == 0", pc_src[1], 0);

    ex_mem_regs_step(&idex, &exmem, &fuw, pc_src, branch_target, 0, &ov, 1);

    ASSERT_EQ_U32("alu_result == 12", reg32_read_u32(&exmem.alu_result), 12);
    ASSERT_EQ_U32("write_data == RT(7)", reg32_read_u32(&exmem.write_data), 7);
    ASSERT_EQ_U32("write_reg_idx == RD(1)", reg32_read_u32(&exmem.write_reg_idx), 1);

    return 0;
}

static int test_exmem_itype_addi_imm_mux(void) {
    printf("=== test_exmem_itype_addi_imm_mux ===\n");

    Id_ex_regs idex; Ex_mem_regs exmem;
    init_id_ex_regs(&idex); init_ex_mem_regs(&exmem);

    word sigw;
    pack_decode_signals_word(sigw, 0, 1, 0, 1, 0, 0, 0, 0, OPS_ADD_);

    // rdata1=1000, imm=9 => result=1009, write_reg_idx=RT=2
    idex_load_minimal(&idex, word_to_u32(sigw), 1000, 777, 9, 0, 2, 1, 200);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    exmem_tick(&idex, &exmem, pc_src, branch_target, 0, &ov);

    ASSERT_EQ_U32("alu_result == 1009", reg32_read_u32(&exmem.alu_result), 1009);
    ASSERT_EQ_U32("write_reg_idx == RT(2)", reg32_read_u32(&exmem.write_reg_idx), 2);

    return 0;
}

static int test_exmem_branch_taken_pc_feedback(void) {
    printf("=== test_exmem_branch_taken_pc_feedback ===\n");

    Id_ex_regs idex; Ex_mem_regs exmem;
    init_id_ex_regs(&idex); init_ex_mem_regs(&exmem);

    word sigw;
    pack_decode_signals_word(sigw, 0, 0, 0, 0, 0, 0, 1, 0, OPS_SUB_);

    // pc_plus4=8, imm_ext=2 => target=8+(2<<2)=16, rdata1==rdata2 => taken
    idex_load_minimal(&idex, word_to_u32(sigw), 0x1234, 0x1234, 2, 0, 0, 0, 8);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;
    Forwarding_unit_writes fuw = {0};

    ex_mem_regs_step(&idex, &exmem, &fuw, pc_src, branch_target, 0, &ov, 0);

    ASSERT_EQ_BIT("pc_src[0]==1 (BRANCH_TARGET)", pc_src[0], 1);
    ASSERT_EQ_BIT("pc_src[1]==0 (BRANCH_TARGET)", pc_src[1], 0);
    ASSERT_EQ_U32("branch_target == 16", word_to_u32(branch_target), 16);

    ex_mem_regs_step(&idex, &exmem, &fuw, pc_src, branch_target, 0, &ov, 1);

    return 0;
}

static int test_exmem_branch_not_taken(void) {
    printf("=== test_exmem_branch_not_taken ===\n");

    Id_ex_regs idex; Ex_mem_regs exmem;
    init_id_ex_regs(&idex); init_ex_mem_regs(&exmem);

    word sigw;
    pack_decode_signals_word(sigw, 0, 0, 0, 0, 0, 0, 1, 0, OPS_SUB_);

    // rdata1 != rdata2 => not taken
    idex_load_minimal(&idex, word_to_u32(sigw), 0x1111, 0x2222, 2, 0, 0, 0, 8);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;
    Forwarding_unit_writes fuw = {0};

    ex_mem_regs_step(&idex, &exmem, &fuw, pc_src, branch_target, 0, &ov, 0);

    ASSERT_EQ_BIT("pc_src[0]==0", pc_src[0], 0);
    ASSERT_EQ_BIT("pc_src[1]==0", pc_src[1], 0);

    return 0;
}

static int test_exmem_flush_bubble(void) {
    printf("=== test_exmem_flush_bubble ===\n");

    Id_ex_regs idex; Ex_mem_regs exmem;
    init_id_ex_regs(&idex); init_ex_mem_regs(&exmem);

    word sigw;
    pack_decode_signals_word(sigw, 1, 0, 0, 1, 1, 1, 1, 0, OPS_ADD_);

    idex_load_minimal(&idex, word_to_u32(sigw), 5, 7, 2, 0, 3, 1, 8);

    pc_ops pc_src = {0, 0};
    word branch_target = {0};
    bit ov = 0;

    exmem_tick(&idex, &exmem, pc_src, branch_target, 1, &ov);

    ASSERT_EQ_BIT("pc_src[0]==0 when ex_flush=1", pc_src[0], 0);
    ASSERT_EQ_BIT("pc_src[1]==0", pc_src[1], 0);
    ASSERT_EQ_U32("alu_result==0", reg32_read_u32(&exmem.alu_result), 0);
    ASSERT_EQ_U32("write_data==0", reg32_read_u32(&exmem.write_data), 0);
    ASSERT_EQ_U32("write_reg_idx==0", reg32_read_u32(&exmem.write_reg_idx), 0);
    ASSERT_EQ_U32("mem_single==0", reg32_read_u32(&exmem.mem_single), 0);
    ASSERT_EQ_U32("wb_single==0", reg32_read_u32(&exmem.wb_single), 0);

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_exmem_rtype_add_basic);
    RUN_TEST(test_exmem_itype_addi_imm_mux);
    RUN_TEST(test_exmem_branch_taken_pc_feedback);
    RUN_TEST(test_exmem_branch_not_taken);
    RUN_TEST(test_exmem_flush_bubble);
    TEST_SUMMARY("EX/MEM Pipeline Register Tests");
    return rc;
}
