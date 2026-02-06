# SCCPU

用 C 语言从逻辑门开始构建一个 5 级流水线 MIPS CPU。

## 这个项目在做什么

用 `AND`、`OR`、`NOT` 等基本逻辑门作为最小构建单元，逐层向上搭建：锁存器 → D 触发器 → 寄存器 → ALU → 译码器 → 流水线数据通路 → 冒险处理。全程不使用 `if` 做控制逻辑，不使用赋值代替寄存器写入，所有时序行为严格遵守二段式时钟（clk=0 采样，clk=1 提交）。

这不是行为级模拟器，也不是 Verilog。它是用软件工程师最熟悉的语言，在硬件的规则下构建 CPU。

## 为什么做这个

经典的计算机组成原理教学存在一个gap：学生能画出流水线框图，能说出 forwarding 的定义，但无法回答"这根 forwarding 线在时钟的哪个相位稳定""flush 注入的 NOP 会不会被 decoder 误识别成有效指令"。这些问题只有在真正构建时才会遇到。

> What I cannot create, I do not understand. — Richard Feynman

## 前置知识

- 能读写 C 语言
- 学过计算机组成原理基础概念（流水线、寄存器、ALU）
- 知道什么是 MIPS 指令格式（R/I/J 型）

## 项目结构

```
includes/
├── gate.h          逻辑门 (NOT, AND, OR, XOR, NOR, XNOR)
├── common.h        基本类型 (bit, byte, word), 位序约定
├── mux.h           2:1 多路选择器
├── dff.h           D 锁存器, 主从 DFF, 边沿检测 DFF
├── reg.h           32-bit 寄存器, 4-寄存器文件
├── alu.h           1-bit ALU → 32-bit ALU (ADD/SUB/SLT/AND/OR/XOR/NOR)
├── isa.h           MIPS 子集指令编码, opcode/funct 位匹配
├── decoder.h       组合逻辑译码器 → 控制信号
├── pc.h            程序计数器
├── im.h            指令存储器
├── dm.h            数据存储器
├── if_id.h         IF 阶段 + IF/ID 流水线寄存器
├── id_ex.h         ID 阶段 + ID/EX 流水线寄存器
├── ex_mem.h        EX 阶段 + EX/MEM 流水线寄存器
├── mem_wb.h        MEM 阶段 + MEM/WB 流水线寄存器
├── wb.h            WB 写回阶段
├── cpu_core.h      顶层连线, 时钟驱动, forwarding, hazard
└── utils.h         编码/解码辅助, 反汇编, 类型转换

tests/
├── test_dff_contract.c             DFF 二段式时钟契约验证
├── test_decoder.c                  译码器单点 + 5000 case 随机回归
├── test_if_id.c                    IF/ID 顺序取指, stall, flush
├── test_id_ex.c                    ID/EX 寄存器读取, flush
├── test_ex_mem.c                   EX 阶段 ALU/分支/flush
├── test_mem_wb.c                   MEM 阶段读写 + byte mask 全覆盖
├── test_wb.c                       WB 写回 + 5000 case fuzz
├── test_dm.c                       数据存储器 + 20000 case golden model 对拍
├── test_parallelism_branch_feedback.c  分支反馈 + 流水线并行性验证
└── test_beq_forwarding.c           BEQ + forwarding 集成测试
```

## 指令集

MIPS 子集，4 个通用寄存器（R0–R3，2-bit 索引）。

| 类型 | 指令 | 格式 | 语义 |
|------|------|------|------|
| R | ADD | ADD rd, rs, rt | rd = rs + rt |
| R | SUB | SUB rd, rs, rt | rd = rs - rt |
| R | AND | AND rd, rs, rt | rd = rs & rt |
| R | OR | OR rd, rs, rt | rd = rs \| rt |
| R | SLT | SLT rd, rs, rt | rd = (rs < rt) ? 1 : 0 |
| I | ADDI | ADDI rt, rs, imm | rt = rs + sign_ext(imm) |
| I | LW | LW rt, imm(rs) | rt = MEM[rs + sign_ext(imm)] |
| I | SW | SW rt, imm(rs) | MEM[rs + sign_ext(imm)] = rt |
| I | BEQ | BEQ rs, rt, offset | if (rs == rt) PC = PC+4 + offset×4 |
| J | J | J target | PC = (PC+4)[31:28] \| (target << 2) |

## 时钟模型

每个周期严格两次调用：

```
cpu_tick():
    phase 0 (clk=0): 从后往前计算所有组合逻辑, 准备 D 端输入
    phase 1 (clk=1): 触发所有 DFF 上升沿, 锁存状态
```

Phase 0 从 WB 到 IF 反向求值，因为 forwarding 和 hazard 信号从后级传回前级，必须在同一拍内稳定。

## 已完成

- 门级基础器件（逻辑门、MUX、DFF、寄存器、ALU）
- 10 条指令的译码和执行
- 5 级流水线数据通路
- Data forwarding（EX→EX, MEM→EX, SW 独立通路）
- BEQ 控制冒险（EX 阶段判定，flush IF/ID 和 ID/EX）

## 未完成

- [ ] J 指令的 PC mux 接线
- [ ] Load-use hazard stall（LW 后紧跟依赖指令需要插 bubble）
- [ ] 可运行的 demo 入口（main.c + 示例程序）

## 设计决策说明

**IM 和 DM 使用"黑盒"实现。** 存储器内部用 `uint8_t` 数组和 `memcpy`，不走门级建模。这是刻意的简化——存储器的物理实现（SRAM cell）不是这个项目要教的东西，把它当作已验证的 IP 使用更合理。

**所有代码放在头文件中。** 全部使用 `static inline`，没有 `.c` 实现文件。这使得每个模块可以被独立 include 和测试，代价是修改任何文件都会触发全量重编译。对于这个规模的项目，这个取舍是可接受的。

**寄存器只有 4 个。** 2-bit 索引而非 MIPS 标准的 5-bit/32 个。这是为了降低调试复杂度，让流水线行为更容易追踪。扩展到 32 个寄存器只需要加宽 mux 和寄存器文件，不涉及架构变更。

## 构建与测试

```bash
# 编译单个测试（以 decoder 测试为例）
gcc -o test_decoder tests/test_decoder.c -I includes -Wall -Wextra

# 运行
./test_decoder
```
