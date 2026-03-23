// test_if_id.c
// IF/ID pipeline register tests: sequential fetch, stall, flush

#include <stdio.h>
#include "../includes/if_id.h"
#include "../includes/im.h"
#include "common_test.h"

// Two-phase tick for IF/ID
static inline void tick_if_id(
    If_id_regs *ifid, const Im_t *im, Pc32_ *pc,
    const If_id_pc_ops *pcops, const If_id_write *wr,
    bit *ov
) {
    if_id_regs_step(ifid, im, pc, pcops, wr, ov, 0);
    if_id_regs_step(ifid, im, pc, pcops, wr, ov, 1);
}

// ======================== Tests ========================

static int test_plus4_basic(void) {
    printf("\n=== test_plus4_basic ===\n");

    Im_t im;
    init_imt(&im);
    im_set_u32(&im, 0, 0x11111111);
    im_set_u32(&im, 1, 0x22222222);
    im_set_u32(&im, 2, 0x33333333);

    Pc32_ pc;
    init_pc32(&pc);

    If_id_regs ifid;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);

    If_id_write wr = {.pc_write = 1, .if_id_write = 1, .if_id_flush = 0};
    If_id_pc_ops ops = {
        .pc_ops_ = {0, 0},
        .branch_target_wire = {0},
        .jump_target_wire = {0},
        .exception_vector_wire = {0}
    };
    bit ov = 0;

    // cycle 0: old_pc=0 -> fetch inst[0], pc_plus4=4
    tick_if_id(&ifid, &im, &pc, &ops, &wr, &ov);

    uint32_t pc_u, inst_u, pc4_u;
    read_pc_u32(&pc, &pc_u);
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);

    ASSERT_EQ_U32("PC after 1st tick == 4", pc_u, 4);
    ASSERT_EQ_U32("IF/ID.instr == im[0]", inst_u, 0x11111111);
    ASSERT_EQ_U32("IF/ID.pc_plus4 == 4", pc4_u, 4);

    // cycle 1: old_pc=4 -> fetch inst[1], pc_plus4=8
    tick_if_id(&ifid, &im, &pc, &ops, &wr, &ov);

    read_pc_u32(&pc, &pc_u);
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);

    ASSERT_EQ_U32("PC after 2nd tick == 8", pc_u, 8);
    ASSERT_EQ_U32("IF/ID.instr == im[1]", inst_u, 0x22222222);
    ASSERT_EQ_U32("IF/ID.pc_plus4 == 8", pc4_u, 8);

    return 0;
}

static int test_stall_pc_and_ifid_hold(void) {
    printf("\n=== test_stall_pc_and_ifid_hold ===\n");

    Im_t im;
    init_imt(&im);
    im_set_u32(&im, 0, 0xAAAAAAAA);
    im_set_u32(&im, 1, 0xBBBBBBBB);

    Pc32_ pc;
    init_pc32(&pc);

    If_id_regs ifid;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);

    If_id_pc_ops ops = {
        .pc_ops_ = {0, 0},
        .branch_target_wire = {0},
        .jump_target_wire = {0},
        .exception_vector_wire = {0}
    };
    bit ov = 0;

    // Normal tick to get some state
    If_id_write wr_run = {.pc_write = 1, .if_id_write = 1, .if_id_flush = 0};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_run, &ov);

    uint32_t pc_u0, inst_u0, pc4_u0;
    read_pc_u32(&pc, &pc_u0);
    read_ifid_instr_u32(&ifid, &inst_u0);
    read_ifid_pc4_u32(&ifid, &pc4_u0);

    // Stall: PC and IF/ID must not change
    If_id_write wr_stall = {.pc_write = 0, .if_id_write = 0, .if_id_flush = 0};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_stall, &ov);

    uint32_t pc_u1, inst_u1, pc4_u1;
    read_pc_u32(&pc, &pc_u1);
    read_ifid_instr_u32(&ifid, &inst_u1);
    read_ifid_pc4_u32(&ifid, &pc4_u1);

    ASSERT_EQ_U32("PC hold when pc_write=0", pc_u1, pc_u0);
    ASSERT_EQ_U32("IF/ID.instr hold when if_id_write=0", inst_u1, inst_u0);
    ASSERT_EQ_U32("IF/ID.pc_plus4 hold when if_id_write=0", pc4_u1, pc4_u0);

    return 0;
}

static int test_flush_inject_nop(void) {
    printf("\n=== test_flush_inject_nop ===\n");

    Im_t im;
    init_imt(&im);
    im_set_u32(&im, 0, 0x12345678);
    im_set_u32(&im, 1, 0x87654321);

    Pc32_ pc;
    init_pc32(&pc);

    If_id_regs ifid;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);

    If_id_pc_ops ops = {
        .pc_ops_ = {0, 0},
        .branch_target_wire = {0},
        .jump_target_wire = {0},
        .exception_vector_wire = {0}
    };
    bit ov = 0;

    If_id_write wr = {.pc_write = 1, .if_id_write = 0, .if_id_flush = 1};
    tick_if_id(&ifid, &im, &pc, &ops, &wr, &ov);

    uint32_t inst_u, pc4_u;
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);

    ASSERT_EQ_U32("IF/ID.instr should be NOP (0)", inst_u, 0);
    ASSERT_EQ_U32("IF/ID.pc_plus4 should be old_pc+4 when flushed", pc4_u, 4);

    return 0;
}

static int test_flush_inject_nop_strict(void) {
    printf("\n=== test_flush_inject_nop_strict ===\n");

    Im_t im;
    init_imt(&im);
    im_set_u32(&im, 0, 0x11111111);
    im_set_u32(&im, 1, 0x22222222);
    im_set_u32(&im, 2, 0x33333333);

    Pc32_ pc;
    init_pc32(&pc);

    If_id_regs ifid;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);

    If_id_pc_ops ops = {
        .pc_ops_ = {0, 0},
        .branch_target_wire = {0},
        .jump_target_wire = {0},
        .exception_vector_wire = {0}
    };
    bit ov = 0;

    // Advance two ticks: PC 0->4->8
    If_id_write wr_run = {.pc_write = 1, .if_id_write = 1, .if_id_flush = 0};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_run, &ov);
    tick_if_id(&ifid, &im, &pc, &ops, &wr_run, &ov);

    uint32_t pc_before;
    read_pc_u32(&pc, &pc_before);
    ASSERT_EQ_U32("PC before flush == 8", pc_before, 8);

    // Flush: inject NOP, pc_plus4 = old_pc+4 = 12
    If_id_write wr_flush = {.pc_write = 1, .if_id_write = 0, .if_id_flush = 1};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_flush, &ov);

    uint32_t inst_u, pc4_u, pc_after;
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);
    read_pc_u32(&pc, &pc_after);

    ASSERT_EQ_U32("IF/ID.instr == NOP", inst_u, 0);
    ASSERT_EQ_U32("IF/ID.pc_plus4 == old_pc+4 (12)", pc4_u, 12);
    ASSERT_EQ_U32("PC after flush tick == 12", pc_after, 12);

    return 0;
}

int main(void) {
    int rc = 0;
    RUN_TEST(test_plus4_basic);
    RUN_TEST(test_stall_pc_and_ifid_hold);
    RUN_TEST(test_flush_inject_nop);
    RUN_TEST(test_flush_inject_nop_strict);
    TEST_SUMMARY("IF/ID Pipeline Register Tests");
    return rc;
}
