#include "includes/cpu_core.h"

int main(void) {
    printf("=== SCCPU Fibonacci Step-by-Step Analysis ===\n");

    Cpu_core cpu;
    init_cpu_c(&cpu);

    const uint32_t program[] = {
        // [0x00] Init F_prev = 0
        enc_addi(1, 0, 0),

        // [0x04] Init F_curr = 1
        enc_addi(2, 0, 1),

        // [0x08] Calculate F_new = 0 + 1 = 1
        // ** Forwarding Test 1 **: R2 依赖上一条 ADDI 的结果 (EX/MEM Hazard)
        enc_r(1, 2, 3, 0, FUNCT_ADD),

        // [0x0C] Shift: F_prev = F_curr (1)
        // ** Forwarding Test 2 **: R2 依赖 0x04 的 ADDI (MEM/WB Hazard 或 寄存器直读)
        enc_addi(1, 2, 0),

        // [0x10] Shift: F_curr = F_new (1)
        // ** Forwarding Test 3 **: R3 依赖 0x08 的 ADD (EX/MEM Hazard)
        enc_addi(2, 3, 0),

        // End
        0, 0, 0, 0
    };

    // Load
    for (int i = 0; i < sizeof(program) / sizeof(uint32_t); i++) {
        word w;
        u32_to_word(program[i], w);
        memcpy(cpu.im.im[i], w, sizeof(word));
    }

    // Run 15 Cycles (足够跑完这 5 条指令)
    for (int cycle = 0; cycle < 15; cycle++) {
        // 我们在这里手动控制打印，方便你看
        cpu_tick(&cpu);
    }

    printf("\n=== Final Register Check ===\n");
    printf("R1 (F_prev) = %d (Expected 1)\n", reg32_read_u32_(&cpu.rf.r1));
    printf("R2 (F_curr) = %d (Expected 1)\n", reg32_read_u32_(&cpu.rf.r2));
    printf("R3 (Temp)   = %d (Expected 1)\n", reg32_read_u32_(&cpu.rf.r3));

    return 0;
}