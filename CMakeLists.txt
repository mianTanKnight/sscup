cmake_minimum_required(VERSION 4.0)
project(sccpu C)

set(CMAKE_C_STANDARD 11)

add_executable(sccpu main.c
        includes/gate.h
        includes/alu.h
        includes/common.h
        includes/dff.h
        includes/reg.h
        includes/mux.h
        includes/pc.h
        includes/isa.h
        includes/decoder.h
        includes/im.h
        tests/test_decoder.c
        includes/if_id.h
        tests/test_if_id.c
        includes/id_ex.h
        tests/test_id_ex.c
        includes/ex_mem.h
        tests/test_ex_mem.c
        tests/common_test.h
        tests/test_parallelism_branch_feedback.c
        includes/dm.h
        tests/test_dm.c
        includes/mem_wb.h
        includes/wb.h
        tests/test_mem_wb.c
        tests/test_wb.c
        includes/cpu_core.h
        includes/utils.h
        tests/test_dff_contract.c
)
