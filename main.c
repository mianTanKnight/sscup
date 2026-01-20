#include <stdio.h>
#include "includes/mux_.h"
#include "includes/reg_.h"
#include "unistd.h"
#include "includes/reg_.h"
#include "includes/alu_.h"
#include "includes/common_.h"
#include "includes/pc_.h"
#include "stdint.h"
#include "includes/id_ex_.h"
#include "includes/isa_.h"
#include "includes/decoder_.h"
#include "includes/dm_.h"

/**
 * SCCPU — Simple Circuit CPU (用 C 写的“电路风格 CPU”)
 *
 * 1. 项目目标 / 哲学
 *    - 这不是一个“软件模拟器”，而是用 C 语言写出来的“硬件描述”。
 *    - 所有模块都尽量按【门电路 + 触发器 + 寄存器 + PC + ALU + RegFile】的方式建模，
 *      让自己从电流 / 电平 / 时钟的视角去真正理解 CPU 是怎么工作的。
 *    - 高级控制流（if / switch / 递归等）尽量不用来描述“数据通路”，
 *      而是用逻辑门 + 多路选择器来表达信号选择，用 DFF 来表达状态。
 *
 * 2. 抽象层约定
 *    - bit      : 一个导线上的电平（0 / 1）。
 *    - byte[8]  : 8bit 总线（约定 b[0] 是最高位 MSB，b[7] 是最低位 LSB）。
 *    - word[32] : 32bit 总线（同样 b[0] 为 MSB，b[31] 为 LSB）。
 *    - for 循环：在语义上理解为“32 根/8 根并行导线同时做相同的事”，
 *      只是用 C 的语法展开，不代表“时间顺序”。
 *
 * 3. 时序约定（全局时钟 + 两阶段）
 *    - 所有时序单元（D 触发器 / 寄存器 / PC / RegFile）都遵守一个统一约定：
 *      - clk=0 阶段：组合逻辑传播，准备好下一拍要写入 D 的值。
 *      - clk 从 0 跳到 1（上升沿）时：D 触发器把 D 锁存到 Q，状态更新。
 *    - dff_deh_step(...) 的实现：
 *      - 用 prev_clk + AND(NOT(prev_clk), clk) 显式表示“上升沿检测”，
 *      - master 在 clk=0 透明，slave 在 edge_clk=1 时更新 Q。
 *    - 所有“记忆”必须经过 DFF / latch，不能藏在 C 的隐式状态里。
 *
 * 4. 模块划分（对应硬件块）
 *    - gate_.h  : 基本逻辑门（NOT / AND / OR / XOR / NOR / NAND / XNOR）。
 *    - mux_.h   : 多路选择器（bit/byte/word 宽度版），用来表达“条件选择”，代替 if。
 *    - dff_.h   : D latch + D flip-flop 行为模型，带 prev_clk 的上升沿检测。
 *    - reg_.h   : 宽度为 8/32 bit 的寄存器，以及由多个寄存器组成的 RegFile。
 *    - alu_.h   : 位片式 ALU（one_bit_alu_），以及 byte_alu_ / word_alu_：
 *                 支持 AND / OR / XOR / NOR / ADD / SUB / SLT / NULL，
 *                 opcode 用 3bit ops[3] 控制，与未来 ISA 一一对应。
 *    - pc_.h    : 程序计数器 PC（32bit），支持 PC+4、分支跳转、复位。
 *
 * 5. ALU 语义（当前版本）
 *    - ops[2:0] 编码约定：
 *        000 AND    逻辑与
 *        001 OR     逻辑或
 *        010 XOR    异或
 *        011 NOR    或非
 *        100 ADD    有符号/无符号加法（两数相加，进位放在 overflow/carry 中）
 *        101 SUB    a - b = a + (~b) + 1，结果按 mod 2^N
 *        110 SLT    有符号比较：ret == 0 或 1（只在最低位输出 1，其余位清零）
 *        111 NULL   输出 0
 *    - SLT 采用经典 two's complement 语义：
 *        less = sign(result_of(a-b)) XOR overflow(a-b)
 *      并仅在最低有效位输出该 less。
 *
 * 6. 电路风格编码约定
 *    - 描述“信号选择”时：优先使用 mux2_1 / byte_mux_2_1 / word_mux_2_1，
 *      避免直接用 if/else 来选路径。
 *    - 描述“宽度为 N 的总线运算”时：使用 for 循环 + 位片逻辑，
 *      在逻辑层理解为“所有位并行执行”。
 *    - 所有状态都必须存放在显式的 struct 中（如 dff_b_, reg32_, pc32_ 等），
 *      禁止依赖函数静态局部变量来“偷偷存状态”。
 *
 *
 * 总之：这个项目的核心不是“写一个能跑的 CPU 模拟器”，
 * 而是用尽量原始、接近电路的方式，用 C 一层一层搭出：
 *   门电路 → 触发器 → 寄存器 → ALU/PC/RegFile → ISA → CPU → 程序。
 * 让自己真正从电路视角理解：CPU 为何如此工作，而不是仅仅会写汇编/系统调用。
 */

int main() {
}
