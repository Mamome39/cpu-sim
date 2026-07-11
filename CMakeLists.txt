# Usage:
#   Configure: cmake -S . -B build
#   Build:     cmake --build build -j$(nproc)
#   Test all:  ctest --output-on-failure (run from build/)
#   One test:  cmake --build build --target <test_name>
#              ./build/<test_name> --gtest_filter=Suite.Test
#   Clean:     rm -rf build/

cmake_minimum_required(VERSION 3.20)
project(cpu-sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── GoogleTest ────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Library targets ───────────────────────────────────────────────────────────
add_library(cpusim_isa STATIC
    src/isa/rv32i/decoder.cpp
)

add_library(cpusim_mem STATIC
    src/memory/flat_mem.cpp
    src/memory/cache.cpp
)
target_include_directories(cpusim_mem PUBLIC include)
target_compile_options(cpusim_mem PRIVATE -Wall -Wextra -Werror)

add_library(cpusim_uarch STATIC
    src/uarch/alu.cpp
    src/uarch/pipeline/fetch.cpp
    src/uarch/pipeline/decode.cpp
    src/uarch/pipeline/forward.cpp
    src/uarch/pipeline/execute.cpp
    src/uarch/pipeline/mem_access.cpp
    src/uarch/pipeline/writeback.cpp
    src/uarch/hazard.cpp
    src/uarch/branch_predictor.cpp
)
target_include_directories(cpusim_uarch PUBLIC include)
target_link_libraries(cpusim_uarch PUBLIC cpusim_isa cpusim_mem)
target_compile_options(cpusim_uarch PRIVATE -Wall -Wextra -Werror)
target_include_directories(cpusim_isa PUBLIC include)
target_compile_options(cpusim_isa PRIVATE -Wall -Wextra -Werror)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
include(GoogleTest)

function(add_unit_test name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE cpusim_isa GTest::gtest_main)
    gtest_discover_tests(${name})
endfunction()

add_unit_test(regfile_test  tests/unit/regfile_test.cpp)
add_unit_test(decoder_test  tests/unit/decoder_test.cpp)

add_executable(alu_test tests/unit/alu_test.cpp)
target_link_libraries(alu_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(alu_test)

add_executable(latch_test tests/unit/latch_test.cpp)
target_link_libraries(latch_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(latch_test)

add_executable(flat_mem_test tests/unit/flat_mem_test.cpp)
target_link_libraries(flat_mem_test PRIVATE cpusim_mem GTest::gtest_main)
gtest_discover_tests(flat_mem_test)

add_executable(cache_test tests/unit/cache_test.cpp)
target_link_libraries(cache_test PRIVATE cpusim_mem GTest::gtest_main)
gtest_discover_tests(cache_test)

add_executable(fetch_test tests/unit/fetch_test.cpp)
target_link_libraries(fetch_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(fetch_test)

add_executable(decode_stage_test tests/unit/decode_stage_test.cpp)
target_link_libraries(decode_stage_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(decode_stage_test)

add_executable(execute_stage_test tests/unit/execute_stage_test.cpp)
target_link_libraries(execute_stage_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(execute_stage_test)

add_executable(mem_stage_test tests/unit/mem_stage_test.cpp)
target_link_libraries(mem_stage_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(mem_stage_test)

add_executable(writeback_stage_test tests/unit/writeback_stage_test.cpp)
target_link_libraries(writeback_stage_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(writeback_stage_test)

add_library(cpusim_sim STATIC
    src/sim/core.cpp
    src/sim/tracer.cpp
    src/sim/elf_loader.cpp
)
target_include_directories(cpusim_sim PUBLIC include)
target_link_libraries(cpusim_sim PUBLIC cpusim_uarch)
target_compile_options(cpusim_sim PRIVATE -Wall -Wextra -Werror)

add_executable(hazard_test tests/unit/hazard_test.cpp)
target_link_libraries(hazard_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(hazard_test)

add_executable(branch_predictor_test tests/unit/branch_predictor_test.cpp)
target_link_libraries(branch_predictor_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(branch_predictor_test)

add_executable(core_test tests/integration/core_test.cpp)
target_link_libraries(core_test PRIVATE cpusim_sim GTest::gtest_main)
gtest_discover_tests(core_test)

add_executable(trace_test tests/integration/trace_test.cpp)
target_link_libraries(trace_test PRIVATE cpusim_sim GTest::gtest_main)
gtest_discover_tests(trace_test)

add_executable(pipeline_test tests/integration/pipeline_test.cpp)
target_link_libraries(pipeline_test PRIVATE cpusim_uarch GTest::gtest_main)
gtest_discover_tests(pipeline_test)

# ── Tools ─────────────────────────────────────────────────────────────────────
add_executable(bench_run tools/bench_run.cpp)
target_link_libraries(bench_run PRIVATE cpusim_sim)
target_compile_options(bench_run PRIVATE -Wall -Wextra -Werror)

# ── Benchmarks (requires riscv64-unknown-elf-gcc) ─────────────────────────────
find_program(RISCV_GCC riscv64-unknown-elf-gcc)
if(RISCV_GCC)
    # libgcc supplies __mulsi3/__divsi3 etc. — RV32I has no hardware
    # multiply/divide, so benchmarks using * / % link against it.
    execute_process(
        COMMAND ${RISCV_GCC} -march=rv32i -mabi=ilp32
                -print-libgcc-file-name
        OUTPUT_VARIABLE RISCV_LIBGCC
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(BENCH_FLAGS
        -march=rv32i -mabi=ilp32 -O1
        -nostdlib -nostartfiles
        -T ${CMAKE_SOURCE_DIR}/benchmarks/link.ld
    )

    # Each entry is a benchmark source in benchmarks/<name>.c, built
    # with the shared start.S into build/<name>.elf.
    set(BENCHMARKS
        bubble_sort
        fibonacci
        crc32
        matmul
        sieve
    )

    set(BENCH_ELVES "")
    foreach(bench ${BENCHMARKS})
        add_custom_command(
            OUTPUT  ${CMAKE_BINARY_DIR}/${bench}.elf
            COMMAND ${RISCV_GCC} ${BENCH_FLAGS}
                    ${CMAKE_SOURCE_DIR}/benchmarks/start.S
                    ${CMAKE_SOURCE_DIR}/benchmarks/${bench}.c
                    ${RISCV_LIBGCC}
                    -o ${CMAKE_BINARY_DIR}/${bench}.elf
            DEPENDS benchmarks/link.ld benchmarks/start.S
                    benchmarks/${bench}.c
            COMMENT "Compiling ${bench} benchmark (RV32I)"
        )
        list(APPEND BENCH_ELVES ${CMAKE_BINARY_DIR}/${bench}.elf)
    endforeach()

    add_custom_target(benchmarks ALL DEPENDS ${BENCH_ELVES})
    message(STATUS "RISC-V toolchain found: ${RISCV_GCC} — benchmarks enabled")
else()
    message(STATUS "riscv64-unknown-elf-gcc not found — benchmarks disabled")
endif()