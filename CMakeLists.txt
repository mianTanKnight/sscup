cmake_minimum_required(VERSION 4.0)
project(sccpu C)

set(CMAKE_C_STANDARD 11)

add_executable(sccpu main.c
        includes/gate_.h
        includes/alu_.h
        includes/common_.h
        includes/dff_.h
        includes/reg_.h
        includes/mux_.h
        includes/pc_.h
        includes/isa_.h
        includes/decoder_.h
        includes/im_.h
        test_decoder.c
        includes/if_id_.h
        test_if_id.c
        includes/id_ex_.h
        test_id_ex.c
        includes/ex_mem.h
        test_ex_mem.c
        includes/common_test.h
        test_parallelism_branch_feedback.c
        includes/dm_.h
        test_dm.c
)
