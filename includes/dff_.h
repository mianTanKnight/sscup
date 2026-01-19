//
// Created by wenshen on 2025/12/4.
//

#ifndef SCCPU_DFF__H
#define SCCPU_DFF__H
#include "common_.h"
#include "gate_.h"

static inline bit d_latch(const bit d, const bit enable, bit OUT_Q, bit OUT_Q_BRA) {
    const bit S = AND(d, enable);
    const bit R = AND(NOT(d), enable);
    // OUT_Q 指的是上一次的结果 也可以是初始结果 也就是Q输出0
    bit stab = 0;
    while (!stab) {
        const bit NEXT_Q = NOR(R, OUT_Q_BRA);
        const bit NEXT_OUT_Q_BRA = NOR(S, OUT_Q);
        stab = AND(NOT(XOR(NEXT_Q, OUT_Q)), NOT(XOR(NEXT_OUT_Q_BRA, OUT_Q_BRA)));
        OUT_Q = mux2_1(NEXT_Q, OUT_Q, stab);
        OUT_Q_BRA = mux2_1(NEXT_OUT_Q_BRA, OUT_Q_BRA, stab);
        // if (NEXT_Q == OUT_Q && NEXT_OUT_Q_BRA == OUT_Q_BRA) {
        //     stab = 1;
        // } else {
        //     OUT_Q = NEXT_Q;
        //     OUT_Q_BRA = NEXT_OUT_Q_BRA;
        // }
    }
    return OUT_Q;
}

typedef bit (*D_LATCH)(bit d, bit enable, bit OUT_Q, bit OUT_Q_BRA);

typedef struct dff {
    D_LATCH master;
    D_LATCH slave;
    bit master_Q;
    bit Q;
} dff_;

typedef struct dff_beh {
    dff_ dff;
    bit prev_clk;
} dff_b_;

static inline void init_dff(dff_ *dff) {
    *dff = (dff_){d_latch, d_latch, 0, 0};
}

static inline void init_dff_deh(dff_b_ *_dff_b) {
    *_dff_b = (dff_b_){{d_latch, d_latch, 0, 0}, 0};
}

static inline void dff_update(dff_ *dff_, const bit clk, const bit d) {
    dff_->master_Q = dff_->master(d, NOT(clk), dff_->master_Q, !dff_->master_Q);
    dff_->Q = dff_->slave(dff_->master_Q, clk, dff_->Q, !dff_->Q);
}

static inline bit dff_deh_step(dff_b_ *_dff_b, const bit clk, const bit d) {
    dff_update(&_dff_b->dff, 0, d);
    dff_update(&_dff_b->dff, AND(NOT(_dff_b->prev_clk), clk), d);
    _dff_b->prev_clk = clk;
    return _dff_b->dff.Q;
}

#endif //SCCPU_DFF__H
