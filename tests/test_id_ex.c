// test_id_ex.c
// 高质量严谨测试：ID/EX 流水线寄存器（two-stage clk）
// 覆盖：
//   A) I-Type imm_ext 符号扩展（正/负）
//   B) 控制信号锁存一致性（R-type / ADDI / LW / SW / BEQ）
//   C) RS/RT/RD 随机选择覆盖（5000+）
//   + stall hold / flush bubble
//
// 说明：本测试严格按你的二段式时钟调用：每个“周期”调用两次：clk=0（准备）+ clk=1（提交）

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "common_test.h"

// 你的工程头文件
#include "../includes/common_.h"
#include "../includes/reg_.h"
#include "../includes/alu_.h"
#include "../includes/isa_.h"
#include "../includes/if_id_.h"
#include "../includes/id_ex_.h"

// -------------------------
// 兼容：如果某些宏没暴露，这里兜底
// -------------------------
#ifndef INST_BIT
// 逻辑 bit n (31..0) -> 存储下标 index = 31 - n
#define INST_BIT(instr, n)  ((instr)[WORD_SIZE - 1 - (n)])
#endif

#ifndef INST_WORD
#define INST_WORD(n) (WORD_SIZE - 1 - (n))
#endif

// -------------------------
// 指令编码（MIPS-like）
// 只要编码的字段正确，你的 INST_BIT 抽取就会命中对应位。
// -------------------------
static inline uint32_t encode_r(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt, uint8_t funct) {
    uint32_t op = 0;
    uint32_t inst = 0;
    inst |= (op & 0x3Fu) << 26;
    inst |= (rs & 0x1Fu) << 21;
    inst |= (rt & 0x1Fu) << 16;
    inst |= (rd & 0x1Fu) << 11;
    inst |= (shamt & 0x1Fu) << 6;
    inst |= (funct & 0x3Fu);
    return inst;
}

static inline uint32_t encode_i(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm) {
    uint32_t inst = 0;
    inst |= ((uint32_t) opcode & 0x3Fu) << 26;
    inst |= ((uint32_t) rs & 0x1Fu) << 21;
    inst |= ((uint32_t) rt & 0x1Fu) << 16;
    inst |= ((uint16_t) imm);
    return inst;
}

// -------------------------
// 读 ID/EX.decode_signals（打包在 reg32_ 里）并解出字段
// 你约定：
//   bit31 reg_dst
//   bit30 alu_src
//   bit29 data_src_to_reg
//   bit28 reg_write
//   bit27 mem_read
//   bit26 mem_write
//   bit25 branch
//   bit24 jump
//   bit23 ops0
//   bit22 ops1
//   bit21 ops2
// -------------------------
typedef struct cs_expect_t {
    bit reg_dst, alu_src, mem_to_reg, reg_write, mem_read, mem_write, branch, jump;
    bit ops0, ops1, ops2;
} cs_expect_t;

static inline cs_expect_t unpack_cs(const Id_ex_regs *idex) {
    cs_expect_t e;
    word w = {0};
    read_reg32((Reg32_ *) &idex->decode_signals, w);

    e.reg_dst = BITN(w, 31);
    e.alu_src = BITN(w, 30);
    e.mem_to_reg = BITN(w, 29);
    e.reg_write = BITN(w, 28);
    e.mem_read = BITN(w, 27);
    e.mem_write = BITN(w, 26);
    e.branch = BITN(w, 25);
    e.jump = BITN(w, 24);
    e.ops0 = BITN(w, 23);
    e.ops1 = BITN(w, 22);
    e.ops2 = BITN(w, 21);
    return e;
}

static inline int assert_cs_eq(const char *prefix, const cs_expect_t *got, const cs_expect_t *exp) {
    char name[256];

#define CHECK(field) do { \
        snprintf(name, sizeof(name), "%s.%s", prefix, #field); \
        if (((got->field)&1) != ((exp->field)&1)) { \
            printf("[FAIL] %s: got=%d expected=%d\n", name, (int)((got->field)&1), (int)((exp->field)&1)); \
            return 1; \
        } else { \
            printf("[PASS] %s\n", name); \
        } \
    } while (0)

    CHECK(reg_dst);
    CHECK(alu_src);
    CHECK(mem_to_reg);
    CHECK(reg_write);
    CHECK(mem_read);
    CHECK(mem_write);
    CHECK(branch);
    CHECK(jump);
    CHECK(ops0);
    CHECK(ops1);
    CHECK(ops2);

#undef CHECK
    return 0;
}

// -------------------------
// 单步：ID/EX two-stage tick
// -------------------------
static inline void idex_tick(
    Id_ex_regs *idex,
    const If_id_regs *ifid,
    const Reg324file_ *rf,
    bit id_ex_write,
    bit id_ex_flush
) {
    Id_ex_write write = {id_ex_write, id_ex_flush};
    // clk=0
    id_ex_regs_step(idex, ifid, rf, &write, 0);
    // clk=1
    id_ex_regs_step(idex, ifid, rf, &write, 1);
}

// -------------------------
// 构造 IF/ID 输入：写 instr / pc_plus4
// -------------------------
static inline void ifid_load(If_id_regs *ifid, uint32_t instr_u32, uint32_t pc_plus4_u32) {
    reg32_write_now(&ifid->instr, instr_u32);
    reg32_write_now(&ifid->pc_plus4, pc_plus4_u32);
}

// 构造 regfile：r0..r3
static inline void rf_load(Reg324file_ *rf, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    reg32_write_now(&rf->r0, r0);
    reg32_write_now(&rf->r1, r1);
    reg32_write_now(&rf->r2, r2);
    reg32_write_now(&rf->r3, r3);
}

// -------------------------
// A) imm_ext 符号扩展测试（正/负）
// -------------------------
static int test_idex_imm_signext(void) {
    printf("\n=== test_idex_imm_signext ===\n");
    If_id_regs ifid;
    Reg324file_ rf;
    Id_ex_regs idex;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);
    init_reg32(&rf.r0);
    init_reg32(&rf.r1);
    init_reg32(&rf.r2);
    init_reg32(&rf.r3);
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    // regfile 随便给点值，不影响 imm_ext
    rf_load(&rf, 0x11111111, 0x22222222, 0x33333333, 0x44444444);

    // 1) ADDI imm=+1
    uint32_t inst_addi_pos = encode_i(OP_ADDI, /*rs*/2, /*rt*/1, /*imm*/(int16_t) 0x0001);
    ifid_load(&ifid, inst_addi_pos, 0x00000010);
    idex_tick(&idex, &ifid, &rf, /*write*/1, /*flush*/0);

    uint32_t imm_ext = reg32_read_u32(&idex.imm_ext);
    ASSERT_EQ_U32("imm_ext for +1", imm_ext, 0x00000001);

    // 2) ADDI imm=-1 (0xFFFF) -> 0xFFFFFFFF
    uint32_t inst_addi_neg = encode_i(OP_ADDI, /*rs*/2, /*rt*/1, /*imm*/(int16_t) 0xFFFF);
    ifid_load(&ifid, inst_addi_neg, 0x00000014);
    idex_tick(&idex, &ifid, &rf, /*write*/1, /*flush*/0);

    imm_ext = reg32_read_u32(&idex.imm_ext);
    ASSERT_EQ_U32("imm_ext for -1", imm_ext, 0xFFFFFFFF);

    return 0;
}

// -------------------------
// B) 控制信号锁存一致性（按指令类型显式期望）
// -------------------------
static int test_idex_control_signals_types(void) {
    printf("\n=== test_idex_control_signals_types ===\n");

    If_id_regs ifid;
    Reg324file_ rf;
    Id_ex_regs idex;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);
    init_reg32(&rf.r0);
    init_reg32(&rf.r1);
    init_reg32(&rf.r2);
    init_reg32(&rf.r3);
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    rf_load(&rf, 0x00000000, 0x11111111, 0x22222222, 0x33333333);

    // R-type ADD: opcode=0, funct=FUNCT_ADD
    uint32_t inst_r_add = encode_r(/*rs*/2, /*rt*/3, /*rd*/1, /*shamt*/0, /*funct*/FUNCT_ADD);
    ifid_load(&ifid, inst_r_add, 0x00000020);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    cs_expect_t got = unpack_cs(&idex);
    cs_expect_t exp = {0};
    exp.reg_dst = 1;
    exp.alu_src = 0;
    exp.mem_to_reg = 0;
    exp.reg_write = 1;
    exp.mem_read = 0;
    exp.mem_write = 0;
    exp.branch = 0;
    exp.jump = 0;
    exp.ops0 = OPS_ADD_[0];
    exp.ops1 = OPS_ADD_[1];
    exp.ops2 = OPS_ADD_[2];
    if (assert_cs_eq("R/ADD", &got, &exp)) return 1;

    // ADDI: opcode=OP_ADDI, ops=ADD, alu_src=1, reg_write=1
    uint32_t inst_addi = encode_i(OP_ADDI, /*rs*/2, /*rt*/1, (int16_t) 0x0007);
    ifid_load(&ifid, inst_addi, 0x00000024);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    got = unpack_cs(&idex);
    exp = (cs_expect_t){0};
    exp.reg_dst = 0;
    exp.alu_src = 1;
    exp.mem_to_reg = 0;
    exp.reg_write = 1;
    exp.mem_read = 0;
    exp.mem_write = 0;
    exp.branch = 0;
    exp.jump = 0;
    exp.ops0 = OPS_ADD_[0];
    exp.ops1 = OPS_ADD_[1];
    exp.ops2 = OPS_ADD_[2];
    if (assert_cs_eq("I/ADDI", &got, &exp)) return 1;

    // LW: mem_to_reg=1, reg_write=1, mem_read=1, ops=ADD, alu_src=1
    uint32_t inst_lw = encode_i(OP_LW, /*rs*/2, /*rt*/1, (int16_t) 0x0010);
    ifid_load(&ifid, inst_lw, 0x00000028);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    got = unpack_cs(&idex);
    exp = (cs_expect_t){0};
    exp.reg_dst = 0;
    exp.alu_src = 1;
    exp.mem_to_reg = 1;
    exp.reg_write = 1;
    exp.mem_read = 1;
    exp.mem_write = 0;
    exp.branch = 0;
    exp.jump = 0;
    exp.ops0 = OPS_ADD_[0];
    exp.ops1 = OPS_ADD_[1];
    exp.ops2 = OPS_ADD_[2];
    if (assert_cs_eq("I/LW", &got, &exp)) return 1;

    // SW: mem_write=1, reg_write=0, mem_read=0, ops=ADD, alu_src=1
    uint32_t inst_sw = encode_i(OP_SW, /*rs*/2, /*rt*/1, (int16_t) 0x0010);
    ifid_load(&ifid, inst_sw, 0x0000002C);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    got = unpack_cs(&idex);
    exp = (cs_expect_t){0};
    exp.reg_dst = 0;
    exp.alu_src = 1;
    exp.mem_to_reg = 0;
    exp.reg_write = 0;
    exp.mem_read = 0;
    exp.mem_write = 1;
    exp.branch = 0;
    exp.jump = 0;
    exp.ops0 = OPS_ADD_[0];
    exp.ops1 = OPS_ADD_[1];
    exp.ops2 = OPS_ADD_[2];
    if (assert_cs_eq("I/SW", &got, &exp)) return 1;

    // BEQ: branch=1, ops=SUB, alu_src=0, reg_write=0
    uint32_t inst_beq = encode_i(OP_BEQ, /*rs*/2, /*rt*/1, (int16_t) 0x0002);
    ifid_load(&ifid, inst_beq, 0x00000030);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    got = unpack_cs(&idex);
    exp = (cs_expect_t){0};
    exp.reg_dst = 0;
    exp.alu_src = 0;
    exp.mem_to_reg = 0;
    exp.reg_write = 0;
    exp.mem_read = 0;
    exp.mem_write = 0;
    exp.branch = 1;
    exp.jump = 0;
    exp.ops0 = OPS_SUB_[0];
    exp.ops1 = OPS_SUB_[1];
    exp.ops2 = OPS_SUB_[2];
    if (assert_cs_eq("I/BEQ", &got, &exp)) return 1;

    return 0;
}

// -------------------------
// C) RS/RT/RD 随机寄存器选择覆盖（大量随机）
//   - regfile r0..r3 填不同魔数
//   - 随机 rs/rt/rd 0..3
//   - 构造 R-type ADD
//   - 断言 read_data1/read_data2/idx 是否匹配
// -------------------------
static int test_idex_random_reg_select(int cases) {
    printf("\n=== test_idex_random_reg_select (%d cases) ===\n", cases);

    If_id_regs ifid;
    Reg324file_ rf;
    Id_ex_regs idex;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);
    init_reg32(&rf.r0);
    init_reg32(&rf.r1);
    init_reg32(&rf.r2);
    init_reg32(&rf.r3);
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    const uint32_t rv[4] = {
        0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u
    };
    rf_load(&rf, rv[0], rv[1], rv[2], rv[3]);

    // 固定 pc_plus4，不影响此测试
    uint32_t pc_p4 = 0x00000100;

    for (int i = 0; i < cases; i++) {
        uint8_t rs = (uint8_t) (rand() & 3);
        uint8_t rt = (uint8_t) (rand() & 3);
        uint8_t rd = (uint8_t) (rand() & 3);

        uint32_t inst = encode_r(rs, rt, rd, 0, FUNCT_ADD);
        ifid_load(&ifid, inst, pc_p4);
        idex_tick(&idex, &ifid, &rf, 1, 0);

        uint32_t got_rs = reg32_read_u32(&idex.read_data1);
        uint32_t got_rt = reg32_read_u32(&idex.read_data2);
        uint32_t got_rs_idx = reg32_read_u32(&idex.rs_idx) & 0x3u;
        uint32_t got_rt_idx = reg32_read_u32(&idex.rt_idx) & 0x3u;
        uint32_t got_rd_idx = reg32_read_u32(&idex.rd_idx) & 0x3u;

        if (got_rs != rv[rs]) {
            printf("[FAIL] case=%d RS data mismatch: rs=%u got=0x%08X exp=0x%08X\n",
                   i, rs, (unsigned) got_rs, (unsigned) rv[rs]);
            return 1;
        }
        if (got_rt != rv[rt]) {
            printf("[FAIL] case=%d RT data mismatch: rt=%u got=0x%08X exp=0x%08X\n",
                   i, rt, (unsigned) got_rt, (unsigned) rv[rt]);
            return 1;
        }
        if (got_rs_idx != rs || got_rt_idx != rt || got_rd_idx != rd) {
            printf("[FAIL] case=%d IDX mismatch: rs=%u rt=%u rd=%u got(rs,rt,rd)=(%u,%u,%u)\n",
                   i, rs, rt, rd, (unsigned) got_rs_idx, (unsigned) got_rt_idx, (unsigned) got_rd_idx);
            return 1;
        }
    }

    PASS("random reg select ok");
    return 0;
}

// -------------------------
// stall hold：id_ex_write=0 且 flush=0 -> 必须保持不变
// -------------------------
static int test_idex_stall_hold(void) {
    printf("\n=== test_idex_stall_hold ===\n");

    If_id_regs ifid;
    Reg324file_ rf;
    Id_ex_regs idex;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);
    init_reg32(&rf.r0);
    init_reg32(&rf.r1);
    init_reg32(&rf.r2);
    init_reg32(&rf.r3);
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    rf_load(&rf, 0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu);

    // 先写入一个已知状态：R-type ADD rs=2 rt=3 rd=1
    uint32_t inst1 = encode_r(2, 3, 1, 0,FUNCT_ADD);
    ifid_load(&ifid, inst1, 0x10);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    uint32_t snap_cs = reg32_read_u32(&idex.decode_signals);
    uint32_t snap_r1 = reg32_read_u32(&idex.read_data1);
    uint32_t snap_r2 = reg32_read_u32(&idex.read_data2);
    uint32_t snap_imm = reg32_read_u32(&idex.imm_ext);

    // 再给 ifid 不同指令，但 stall：write=0
    uint32_t inst2 = encode_i(OP_ADDI, 1, 0, (int16_t) 0x1234);
    ifid_load(&ifid, inst2, 0x14);
    idex_tick(&idex, &ifid, &rf, /*write*/0, /*flush*/0);

    ASSERT_EQ_U32("cs hold", reg32_read_u32(&idex.decode_signals), snap_cs);
    ASSERT_EQ_U32("r1 hold", reg32_read_u32(&idex.read_data1), snap_r1);
    ASSERT_EQ_U32("r2 hold", reg32_read_u32(&idex.read_data2), snap_r2);
    ASSERT_EQ_U32("imm hold", reg32_read_u32(&idex.imm_ext), snap_imm);

    return 0;
}

// -------------------------
// flush bubble：id_ex_flush=1 -> 所有副作用信号与数据/索引清 0（pc_plus4 策略你已固定：可更新）
// -------------------------
static int test_idex_flush_bubble(void) {
    printf("\n=== test_idex_flush_bubble ===\n");

    If_id_regs ifid;
    Reg324file_ rf;
    Id_ex_regs idex;
    init_reg32(&ifid.instr);
    init_reg32(&ifid.pc_plus4);
    init_reg32(&rf.r0);
    init_reg32(&rf.r1);
    init_reg32(&rf.r2);
    init_reg32(&rf.r3);
    init_reg32(&idex.decode_signals);
    init_reg32(&idex.imm_ext);
    init_reg32(&idex.pc_plus4);
    init_reg32(&idex.read_data1);
    init_reg32(&idex.read_data2);
    init_reg32(&idex.rs_idx);
    init_reg32(&idex.rt_idx);
    init_reg32(&idex.rd_idx);

    rf_load(&rf, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);

    // 先正常打一拍，建立非零状态
    uint32_t inst1 = encode_i(OP_LW, 2, 1, (int16_t) 0x0010);
    ifid_load(&ifid, inst1, 0x100);
    idex_tick(&idex, &ifid, &rf, 1, 0);

    // 再 flush：期望 bubble
    uint32_t inst2 = encode_r(2, 3, 1, 0,FUNCT_ADD);
    ifid_load(&ifid, inst2, 0x104);
    idex_tick(&idex, &ifid, &rf, /*write*/0, /*flush*/1);

    ASSERT_EQ_U32("decode_signals bubble == 0", reg32_read_u32(&idex.decode_signals), 0u);
    ASSERT_EQ_U32("read_data1 bubble == 0", reg32_read_u32(&idex.read_data1), 0u);
    ASSERT_EQ_U32("read_data2 bubble == 0", reg32_read_u32(&idex.read_data2), 0u);
    ASSERT_EQ_U32("imm_ext bubble == 0", reg32_read_u32(&idex.imm_ext), 0u);
    ASSERT_EQ_U32("rs_idx bubble == 0", reg32_read_u32(&idex.rs_idx) & 3u, 0u);
    ASSERT_EQ_U32("rt_idx bubble == 0", reg32_read_u32(&idex.rt_idx) & 3u, 0u);
    ASSERT_EQ_U32("rd_idx bubble == 0", reg32_read_u32(&idex.rd_idx) & 3u, 0u);

    // pc_plus4 策略：你当前是“即使 flush 也照常锁存 pc_plus4”
    // 这里给个严格断言：pc_plus4 == ifid.pc_plus4 (0x104)
    ASSERT_EQ_U32("pc_plus4 policy: updated even on flush", reg32_read_u32(&idex.pc_plus4), 0x00000104u);

    return 0;
}

// int main(void) {
//     // 为随机测试固定种子，避免每次输出不一致（你也可以换成 time(NULL)）
//     srand(0xC0FFEEu);
//
//     int rc = 0;
//     rc |= test_idex_imm_signext();
//     rc |= test_idex_control_signals_types();
//     rc |= test_idex_stall_hold();
//     rc |= test_idex_flush_bubble();
//     rc |= test_idex_random_reg_select(5000);
//
//     if (rc == 0) {
//         printf("\nALL ID/EX TESTS PASSED ✅\n");
//         return 0;
//     } else {
//         printf("\nSOME ID/EX TESTS FAILED ❌\n");
//         return 1;
//     }
// }
