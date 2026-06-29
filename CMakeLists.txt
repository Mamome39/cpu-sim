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
)
target_include_directories(cpusim_mem PUBLIC include)
target_compile_options(cpusim_mem PRIVATE -Wall -Wextra -Werror)

add_library(cpusim_uarch STATIC
    src/uarch/alu.cpp
    src/uarch/pipeline/fetch.cpp
    src/uarch/pipeline/decode.cpp
    src/uarch/pipeline/execute.cpp
    src/uarch/pipeline/mem_access.cpp
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