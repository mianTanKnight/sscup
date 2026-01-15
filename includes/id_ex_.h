//
// Created by wenshen on 2025/12/26.
//

#ifndef SCCPU_ID_EX__H
#define SCCPU_ID_EX__H

#include "decoder_.h"
#include "reg_.h"
#include "if_id_.h"

typedef struct id_ex_regs {
    // Form decode
    // control_signals decode_signals;
    // 这里使用寄存器 储存 control_signals
    // 并且从高位(index0) 开始
    // typedef struct {
    //     bit reg_dst;
    //     bit alu_src;
    //     bit data_src_to_reg;
    //     bit reg_write;
    //     bit mem_read;
    //     bit mem_write;
    //     bit branch;
    //     bit jump;
    //     ops ops_;
    // }  control_signals;
    //  reg_dst 在最高位(0)
    Reg32_ decode_signals;

    // Data Values
    Reg32_ read_data1; // RS
    Reg32_ read_data2; // RT
    Reg32_ imm_ext; // I
    // Indexes
    Reg32_ rs_idx;
    Reg32_ rt_idx;
    Reg32_ rd_idx;
    // Pc-info
    Reg32_ pc_plus4;
} Id_ex_regs;


static inline void
init_id_ex_regs(Id_ex_regs *regs) {
    init_reg32(&regs->decode_signals);
    init_reg32(&regs->read_data1);
    init_reg32(&regs->read_data2);
    init_reg32(&regs->imm_ext);
    init_reg32(&regs->rs_idx);
    init_reg32(&regs->rt_idx);
    init_reg32(&regs->rd_idx);
    init_reg32(&regs->pc_plus4);
}


static inline void
id_ex_regs_step(Id_ex_regs *id_ex_regs, const If_id_regs *if_id_regs, const Reg324file_ *reg324_file, bit id_ex_write,
                bit id_ex_flush,
                const bit clk) {
    // read data of if_id_regs
    word instr = {0};
    word pc_plus4 = {0};
    read_reg32(&if_id_regs->instr, instr);
    read_reg32(&if_id_regs->pc_plus4, pc_plus4);

    //signals 源至 instr(if_id_regs->instr {)
    const Control_signals signals = decode(instr);

    // Read out 0 to 3 reg
    word r0v = {0};
    word r1v = {0};
    word r2v = {0};
    word r3v = {0};
    read_reg32(&reg324_file->r0, r0v);
    read_reg32(&reg324_file->r1, r1v);
    read_reg32(&reg324_file->r2, r2v);
    read_reg32(&reg324_file->r3, r3v);

    // Read out rs, rt, rd ops
    // R0 → 00
    // R1 → 01
    // R2 → 10
    // R3 → 11
    bit rs_ops[2] = {0};
    bit rt_ops[2] = {0};
    bit rd_ops[2] = {0};
    // 25:21 -> RS, 20:16 -> RT, 15:11 -> RD
    // 四位寄存器 只会用到最后的两位
    rs_ops[0] = INST_BIT(instr, 22);
    rs_ops[1] = INST_BIT(instr, 21);
    rt_ops[0] = INST_BIT(instr, 17);
    rt_ops[1] = INST_BIT(instr, 16);
    rd_ops[0] = INST_BIT(instr, 12);
    rd_ops[1] = INST_BIT(instr, 11);

    // Data
    word rs = {0};
    word rt = {0};
    word imm_ext = {0};
    word rs_index = {0};
    word rt_index = {0};
    word rd_index = {0};

    // Mux 4 to 1
    word tmp_mux_0 = {0};
    word tmp_mux_1 = {0};

    // RS
    word_mux_2_1(r0v, r2v, rs_ops[0], tmp_mux_0);
    word_mux_2_1(r1v, r3v, rs_ops[0], tmp_mux_1);
    word_mux_2_1(tmp_mux_0, tmp_mux_1, rs_ops[1], rs);

    word_mux_2_1(rs, WORD_ZERO, id_ex_flush, rs);

    // RT
    word_mux_2_1(r0v, r2v, rt_ops[0], tmp_mux_0);
    word_mux_2_1(r1v, r3v, rt_ops[0], tmp_mux_1);
    word_mux_2_1(tmp_mux_0, tmp_mux_1, rt_ops[1], rt);

    word_mux_2_1(rt, WORD_ZERO, id_ex_flush, rt);

    // IMM-EXT
    // [31:26]  [25:21]  [20:16]  [15:0]
    //  Opcode    RS       RT       Immediate
    //  6bits     5bits    5bits     16bits
    // Ext-Fill  id_ex_flush is 1 set 0
    for (int i = 31; i > 15; i--)
        imm_ext[INST_WORD(i)] = AND(INST_BIT(instr, 15), NOT(id_ex_flush));
    for (int i = 15; i >= 0; i--)
        imm_ext[INST_WORD(i)] = AND(INST_BIT(instr, i), NOT(id_ex_flush));


    // index
    rs_index[INST_WORD(0)] = AND(rs_ops[1], NOT(id_ex_flush));
    rs_index[INST_WORD(1)] = AND(rs_ops[0], NOT(id_ex_flush));

    rt_index[INST_WORD(0)] = AND(rt_ops[1], NOT(id_ex_flush));
    rt_index[INST_WORD(1)] = AND(rt_ops[0], NOT(id_ex_flush));

    rd_index[INST_WORD(0)] = AND(rd_ops[1], NOT(id_ex_flush));
    rd_index[INST_WORD(1)] = AND(rd_ops[0], NOT(id_ex_flush));

    // CALL_STEP
    word out = {0};

    //decode_signals
    word decode_signals_word = {0};
    decode_signals_word[INST_WORD(31)] = AND(signals.reg_dst, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(30)] = AND(signals.alu_src, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(29)] = AND(signals.data_src_to_reg, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(28)] = AND(signals.reg_write, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(27)] = AND(signals.mem_read, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(26)] = AND(signals.mem_write, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(25)] = AND(signals.branch, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(24)] = AND(signals.jump, NOT(id_ex_flush));
    decode_signals_word[INST_WORD(23)] = AND(signals.ops_[0], NOT(id_ex_flush));
    decode_signals_word[INST_WORD(22)] = AND(signals.ops_[1], NOT(id_ex_flush));
    decode_signals_word[INST_WORD(21)] = AND(signals.ops_[2], NOT(id_ex_flush));

    reg32_step(&id_ex_regs->decode_signals, OR(id_ex_write, id_ex_flush), decode_signals_word, out, clk);
    reg32_step(&id_ex_regs->read_data1, OR(id_ex_write, id_ex_flush), rs, out, clk);
    reg32_step(&id_ex_regs->read_data2, OR(id_ex_write, id_ex_flush), rt, out, clk);
    reg32_step(&id_ex_regs->imm_ext, OR(id_ex_write, id_ex_flush), imm_ext, out, clk);
    reg32_step(&id_ex_regs->rs_idx, OR(id_ex_write, id_ex_flush), rs_index, out, clk);
    reg32_step(&id_ex_regs->rt_idx, OR(id_ex_write, id_ex_flush), rt_index, out, clk);
    reg32_step(&id_ex_regs->rd_idx, OR(id_ex_write, id_ex_flush), rd_index, out, clk);

    reg32_step(&id_ex_regs->pc_plus4, OR(id_ex_write, id_ex_flush), pc_plus4, out, clk);
}

/*********************************************Macro***************************************************************/

#define  GET_REG_DST_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(31)].dff.Q)
#define  GET_ALU_SRC_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(30)].dff.Q)
#define  GET_DATA_SRC_TO_REG_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(29)].dff.Q)
#define  GET_REG_WRITE_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(28)].dff.Q)
#define  GET_MEM_READ_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(27)].dff.Q)
#define  GET_MEM_WRITE_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(26)].dff.Q)
#define  GET_BRANCH_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(25)].dff.Q)
#define  GET_JUMP_OF_SIGNALS(r_ptr) ((r_ptr)->dffs[INST_WORD(24)].dff.Q)

#define  GET_OPS_OF_SIGNALS(r_ptr, ret_ops) do { \
(ret_ops)[0] = (r_ptr)->dffs[INST_WORD(23)].dff.Q; \
(ret_ops)[1] = (r_ptr)->dffs[INST_WORD(22)].dff.Q; \
(ret_ops)[2] = (r_ptr)->dffs[INST_WORD(21)].dff.Q; \
} while(0)


#endif //SCCPU_ID_EX__H
