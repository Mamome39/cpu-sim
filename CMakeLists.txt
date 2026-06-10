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