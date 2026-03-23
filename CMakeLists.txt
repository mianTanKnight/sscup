cmake_minimum_required(VERSION 3.10)
project(sccpu C)

set(CMAKE_C_STANDARD 11)

# Main executable (placeholder - not built until demo is ready)
# add_executable(sccpu main.c)
# target_include_directories(sccpu PRIVATE includes)

# ======================== Test Targets ========================
# Each test builds as its own independent executable

set(TEST_NAMES
    test_gate
    test_mux
    test_alu
    test_reg
    test_pc
    test_dff_contract
    test_decoder
    test_if_id
    test_id_ex
    test_ex_mem
    test_mem_wb
    test_wb
    test_dm
    test_parallelism_branch_feedback
    test_beq_forwarding
)

foreach(TEST_NAME ${TEST_NAMES})
    add_executable(${TEST_NAME} tests/${TEST_NAME}.c)
    target_include_directories(${TEST_NAME} PRIVATE includes)
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wno-unused-function -Wno-unused-variable)
endforeach()
