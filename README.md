# SCCPU - Simple Circuit CPU

         "What I cannot create, I do not understand." — Richard Feynman

SCCPU 是一个 从零构建的、RTL (寄存器传输级) 精度 的 32 位流水线 CPU 模拟器。
与常见的指令集模拟器 (Instruction Set Simulator, ISS) 不同，本项目拒绝使用高级语言特性来模拟硬件行为。我们不模拟指令的结果，我们模拟 电路的流动。

# 核心设计 (Design Philosophy)

## 本项目的核心目标是填平软件与硬件认知的鸿沟。为了达成这一目标，我们遵循以下严苛的工程纪律：

#### 电路化编程 (Circuit-Oriented Programming):

控制逻辑完全由与或非门 (AND/OR/NOT) 和多路选择器 (MUX) 构建。

所有的状态必须存储在显式的触发器 (DFF) 或寄存器 (Reg) 中。

for 循环不代表迭代，代表 空间上的并行连线 (Instantiation)。

严格的时序模型 (Strict Timing Model):

     采用 二段式仿真 (Two-Phase Simulation) 机制：

          CLK=0: 组合逻辑计算 (Combinational Logic Evaluation)。计算导线 (Wire) 电平。

          CLK=1: 时序逻辑更新 (Sequential Logic Update)。触发器 (State) 翻转。

严格区分 Wire (瞬时导线) 与 Reg (延时存储)。

#### 逆序更新策略 (Back-to-Front Update):

为了在 C 语言的顺序执行中模拟硬件的并发性，流水线寄存器的更新严格遵循 WB -> MEM -> EX -> ID -> IF 的顺序，防止数据穿透 (Shoot-through)。

# 架构规格 (Architecture Specs)

ISA: MIPS-like 32-bit RISC (精简指令集)

Pipeline: 经典 5 级流水线 (IF, ID, EX, MEM, WB)

Addressing: 字节寻址 (Byte Addressing)，强制字对齐 (Strict Word Alignment)。

Register File: 4 个 32-bit 通用寄存器 (R0-R3)。

Memory: 分离的指令内存 (IM) 和数据内存 (DM)。
