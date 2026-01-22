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
    printf("Hello, Simple Circuit CPU.");
    Cpu_core c = {0};
    init_cpu_c(&c);
    cpu_dump(&c);
    return 0;
}
