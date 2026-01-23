#include <stdio.h>
// #include "includes/mux_.h"
// #include "includes/reg_.h"
// #include "unistd.h"
// #include "includes/reg_.h"
// #include "includes/alu_.h"
// #include "includes/common_.h"
// #include "includes/pc_.h"
// #include "stdint.h"
// #include "includes/id_ex_.h"
// #include "includes/isa_.h"
// #include "includes/decoder_.h"
// #include "includes/dm_.h"
#include "includes/cpu_core.h"

int main(void) {
    printf("=== SCCPU System Integration Test ===\n");

    // 1. 初始化 CPU
    Cpu_core cpu;
    init_cpu_c(&cpu);

    // 2. 准备程序 (机器码)
    uint32_t program[] = {
        // 0x00: ADDI R1, R0, 10
        enc_addi(1, 0, 10),
        // 0x04: NOP (全0)
        0x00000000,
        // 0x08: NOP
        0x00000000,
        // 0x0C: NOP
        0x00000000,
        // 0x10: ADDI R2, R0, 20
        enc_addi(2, 0, 20),
        // NOPs...
        0, 0, 0,
        // 0x20: ADD R3, R1, R2
        enc_r(1, 2, 3, 0, FUNCT_ADD), // RS=1, RT=2, RD=3
        // NOPs...
        0, 0, 0,
        // 0x30: SW R3, 100(R0)
        enc_i(OP_SW, 0, 3, 100), // RS=0(Base), RT=3(Src), Imm=100
        // 0x34: NOPs (等待写入完成)
        0, 0, 0,
        // 0x40: LW R2, 100(R0) (读回 R2 验证)
        enc_i(OP_LW, 0, 2, 100), // RS=0(Base), RT=2(Dest), Imm=100
        // End loop
        0, 0, 0
    };

    for (int i = 0; i < sizeof(program)/sizeof(uint32_t); i++) {
        word w;
        u32_to_word(program[i], w);
        memcpy(cpu.im.im[i], w, sizeof(word));
    }
    // 4. 启动时钟 (跑 20 个周期看看)
    for (int cycle = 0; cycle < 20; cycle++) {
        printf("\n--- Cycle %d (CLK=0) ---\n", cycle);
        falling_edge(&cpu);
        printf("--- Cycle %d (CLK=1) ---\n", cycle);
        rising_edge(&cpu);
        // 可以在这里加个断点或者 sleep
    }
    // 5. 最终检查
    uint32_t r3_val = reg32_read_u32_(&cpu.rf.r3);
    printf("\nFinal Result: R3 = %d (Expected 30)\n", r3_val);



    return 0;
}
