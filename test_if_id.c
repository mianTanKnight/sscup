//
// Created by wenshen on 2025/12/23.
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "includes/common_.h"
#include "includes/reg_.h"
#include "includes/im_.h"
#include "includes/pc_.h"
#include "includes/if_id_.h"
#include "includes/common_test.h"


// 每拍严格二段式
static inline void tick_if_id(
    If_id_regs *ifid, const Im_t *im, Pc32_ *pc,
    const If_id_pc_ops *pcops, const If_id_write *wr,
    bit *ov
) {
    if_id_regs_step(ifid, im, pc, pcops, wr, ov, 0);
    if_id_regs_step(ifid, im, pc, pcops, wr, ov, 1);
}

// ------------------------------------------------------------
// 测试用：构造 pc_ops（注意：你的结构体里是 const word 成员，得用静态/临时对象初始化）
// ------------------------------------------------------------
static inline If_id_pc_ops make_pcops_plus4(void) {
    // 注意：你 if_id_pc_ops 里 word 成员是 const 数组，必须在初始化时给出
    static word z = {0};
    If_id_pc_ops ops = {
        .pc_ops_ = {0, 0},
        .branch_target_wire = {0},
        .jump_target_wire = {0},
        .exception_vector_wire = {0}
    };
    (void) z;
    return ops;
}

static inline If_id_pc_ops make_pcops_branch(uint32_t target) {
    If_id_pc_ops ops = {
        .pc_ops_ = {1, 0},
        .branch_target_wire = {0},
        .jump_target_wire = {0},
        .exception_vector_wire = {0}
    };
    word_from_u32(target, ops.branch_target_wire); // 这行如果编译器不让转，说明你应该把 struct 成员改成指针形式
    return ops;
}

// ------------------------------------------------------------
// 1) 顺序取指 + PC+4
// ------------------------------------------------------------
static int test_plus4_basic(void) {
    printf("\n=== test_plus4_basic ===\n");

    Im_t im;
    init_imt(&im);

    // program: addr 0,4,8,...
    // 用“明显不同”的指令值当样本
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

    // cycle 0: old_pc=0 -> fetch inst[0], pc_plus4=4, pc becomes 4
    tick_if_id(&ifid, &im, &pc, &ops, &wr, &ov);

    uint32_t pc_u, inst_u, pc4_u;
    read_pc_u32(&pc, &pc_u);
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);

    ASSERT_EQ_U32("PC after 1st tick == 4", pc_u, 4);
    ASSERT_EQ_U32("IF/ID.instr == im[0]", inst_u, 0x11111111);
    ASSERT_EQ_U32("IF/ID.pc_plus4 == 4", pc4_u, 4);

    // cycle 1: old_pc=4 -> fetch inst[1], pc_plus4=8, pc becomes 8
    tick_if_id(&ifid, &im, &pc, &ops, &wr, &ov);

    read_pc_u32(&pc, &pc_u);
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);

    ASSERT_EQ_U32("PC after 2nd tick == 8", pc_u, 8);
    ASSERT_EQ_U32("IF/ID.instr == im[1]", inst_u, 0x22222222);
    ASSERT_EQ_U32("IF/ID.pc_plus4 == 8", pc4_u, 8);
}

// ------------------------------------------------------------
// 2) stall：pc_write=0 / if_id_write=0
// ------------------------------------------------------------
static void test_stall_pc_and_ifid_hold(void) {
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

    // 先正常推进一拍，让 PC=4，IF/ID 有 im[0]
    If_id_write wr_run = {.pc_write = 1, .if_id_write = 1, .if_id_flush = 0};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_run, &ov);

    uint32_t pc_u0, inst_u0, pc4_u0;
    read_pc_u32(&pc, &pc_u0);
    read_ifid_instr_u32(&ifid, &inst_u0);
    read_ifid_pc4_u32(&ifid, &pc4_u0);

    // 再 stall：PC 和 IF/ID 都不应变化
    If_id_write wr_stall = {.pc_write = 0, .if_id_write = 0, .if_id_flush = 0};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_stall, &ov);

    uint32_t pc_u1, inst_u1, pc4_u1;
    read_pc_u32(&pc, &pc_u1);
    read_ifid_instr_u32(&ifid, &inst_u1);
    read_ifid_pc4_u32(&ifid, &pc4_u1);

    ASSERT_EQ_U32("PC hold when pc_write=0", pc_u1, pc_u0);
    ASSERT_EQ_U32("IF/ID.instr hold when if_id_write=0", inst_u1, inst_u0);
    ASSERT_EQ_U32("IF/ID.pc_plus4 hold when if_id_write=0", pc4_u1, pc4_u0);
}

// ------------------------------------------------------------
// 3) flush：IF/ID.instr 变 NOP（推荐 pc_plus4 也一致刷新）
// ------------------------------------------------------------
static void test_flush_inject_nop(void) {
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

    // flush=1，允许写 IF/ID（即使 if_id_write=0，也推荐 flush 强制写）
    // 你当前实现：instr 会被写 NOP，但 pc_plus4 可能不更新（这是你应修的点）
    If_id_write wr = {.pc_write = 1, .if_id_write = 0, .if_id_flush = 1};

    tick_if_id(&ifid, &im, &pc, &ops, &wr, &ov);

    uint32_t inst_u, pc4_u;
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);

    ASSERT_EQ_U32("IF/ID.instr should be NOP (0)", inst_u, 0);

    // 这一条你需要决定“flush 时 pc_plus4 要不要也刷新”
    // 如果你修成 flush 强制写 0：
    // ASSERT_EQ_U32("IF/ID.pc_plus4 should be 0 when flushed", pc4_u, 0);

    // 或者你修成 flush 时写入这拍的 pc_plus4_wire（更贴“槽位携带信息”）：
    ASSERT_EQ_U32("IF/ID.pc_plus4 should be old_pc+4 when flushed", pc4_u, 4);

    // 目前你代码大概率会让 pc4_u 保持旧值（这条测试会提醒你改）
    printf("    [INFO] pc_plus4 after flush = 0x%08X (decide desired policy and assert it)\n", (unsigned) pc4_u);
}


static void test_flush_inject_nop_strict(void) {
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

    // 先推进两拍：PC 0->4->8
    If_id_write wr_run = {.pc_write = 1, .if_id_write = 1, .if_id_flush = 0};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_run, &ov);
    tick_if_id(&ifid, &im, &pc, &ops, &wr_run, &ov);

    uint32_t pc_before;
    read_pc_u32(&pc, &pc_before);
    ASSERT_EQ_U32("PC before flush == 8", pc_before, 8);

    // flush 当拍：注入 NOP，但 pc_plus4 仍锁存 old_pc+4 = 12
    If_id_write wr_flush = {.pc_write = 1, .if_id_write = 0, .if_id_flush = 1};
    tick_if_id(&ifid, &im, &pc, &ops, &wr_flush, &ov);

    uint32_t inst_u, pc4_u, pc_after;
    read_ifid_instr_u32(&ifid, &inst_u);
    read_ifid_pc4_u32(&ifid, &pc4_u);
    read_pc_u32(&pc, &pc_after);

    ASSERT_EQ_U32("IF/ID.instr == NOP", inst_u, 0);
    ASSERT_EQ_U32("IF/ID.pc_plus4 == old_pc+4 (12)", pc4_u, 12);
    ASSERT_EQ_U32("PC after flush tick == 12", pc_after, 12);
}

// int main1(void) {
//     test_plus4_basic();
//     test_stall_pc_and_ifid_hold();
//     test_flush_inject_nop();
//     test_flush_inject_nop_strict();
//     return 0;
// }
