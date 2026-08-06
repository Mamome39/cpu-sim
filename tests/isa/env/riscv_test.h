// cpu-sim replacement for riscv-test-env's p/riscv_test.h.
//
// Upstream's environment boots through machine-mode CSRs: it installs
// an mtvec trap vector, programs PMP/SATP, and returns to the test
// body via mret, then exits by trapping on ecall. cpu-sim implements
// RV32I only -- no CSRs, no traps, no privilege modes -- so none of
// that can run here.
//
// This file provides the same RVTEST_* interface using nothing but
// RV32I, so the vendored rv32ui/rv64ui test bodies assemble
// unmodified. The startup and exit sequence mirrors
// benchmarks/start.S: set up a stack, run, then write the result to
// tohost (which Spike polls) and halt with EBREAK (which cpu-sim
// halts on).
//
// Exit encoding matches upstream so Spike reports pass/fail the same
// way: tohost = 1 means pass, tohost = (TESTNUM << 1) | 1 means the
// test numbered TESTNUM failed.

#ifndef _ENV_CPUSIM_RISCV_TEST_H
#define _ENV_CPUSIM_RISCV_TEST_H

//-----------------------------------------------------------------------
// Test number register
//-----------------------------------------------------------------------
// test_macros.h and the test bodies both write this. Upstream uses gp;
// keeping the same register keeps the vendored sources unmodified.

#define TESTNUM gp

//-----------------------------------------------------------------------
// ISA selection macros
//-----------------------------------------------------------------------
// Upstream uses these to emit per-ISA enable code (FP, vector, ...).
// RV32I user-mode tests need no setup, so `init` expands to nothing.
// RVTEST_CODE_BEGIN still invokes it, matching upstream structure.

#define RVTEST_RV32U                                                    \
  .macro init;                                                          \
  .endm

#define RVTEST_RV64U RVTEST_RV32U

//-----------------------------------------------------------------------
// Register initialisation
//-----------------------------------------------------------------------
// Zero every writable register before the body runs. Upstream does the
// same, and it keeps our trace and Spike's agreeing from the first
// instruction regardless of reset state.

#define INIT_XREG                                                       \
        li x1, 0;                                                       \
        li x2, 0;                                                       \
        li x3, 0;                                                       \
        li x4, 0;                                                       \
        li x5, 0;                                                       \
        li x6, 0;                                                       \
        li x7, 0;                                                       \
        li x8, 0;                                                       \
        li x9, 0;                                                       \
        li x10, 0;                                                      \
        li x11, 0;                                                      \
        li x12, 0;                                                      \
        li x13, 0;                                                      \
        li x14, 0;                                                      \
        li x15, 0;                                                      \
        li x16, 0;                                                      \
        li x17, 0;                                                      \
        li x18, 0;                                                      \
        li x19, 0;                                                      \
        li x20, 0;                                                      \
        li x21, 0;                                                      \
        li x22, 0;                                                      \
        li x23, 0;                                                      \
        li x24, 0;                                                      \
        li x25, 0;                                                      \
        li x26, 0;                                                      \
        li x27, 0;                                                      \
        li x28, 0;                                                      \
        li x29, 0;                                                      \
        li x30, 0;                                                      \
        li x31, 0;

//-----------------------------------------------------------------------
// Begin macro
//-----------------------------------------------------------------------
// Entry lands in .text.init, which link.ld places first at the load
// base so _start is the first instruction executed.
//
// The `j 1f` is load-bearing. The test body assembles into .text
// while this prologue sits in .text.init, and the two sections are
// not necessarily adjacent -- alignment padding can leave a gap of
// dead bytes between them. Upstream crosses that gap with `mret`; we
// use a plain jump. Falling through instead would execute the
// padding.

#define RVTEST_CODE_BEGIN                                               \
        .section .text.init, "ax";                                      \
        .align 6;                                                       \
        .globl _start;                                                  \
_start:                                                                 \
        INIT_XREG;                                                      \
        la sp, _stack_top;                                              \
        li TESTNUM, 0;                                                  \
        init;                                                           \
        j 1f;                                                           \
        .section .text;                                                 \
1:

//-----------------------------------------------------------------------
// End macro
//-----------------------------------------------------------------------
// Unreachable: every test body ends in TEST_PASSFAIL, which always
// takes the RVTEST_PASS or RVTEST_FAIL path. Upstream emits `unimp`
// here; EBREAK is the equivalent backstop that cpu-sim halts on
// rather than treating as an illegal encoding.

#define RVTEST_CODE_END                                                 \
        ebreak

//-----------------------------------------------------------------------
// Pass/fail macros
//-----------------------------------------------------------------------
// Write the result word to tohost, then EBREAK. Spike exits when a
// nonzero value lands in tohost; cpu-sim runs one more instruction and
// halts on the EBREAK. spike_diff.sh already strips that trailing
// EBREAK before comparing, so the two traces line up exactly.
//
// The leading `fence` is kept from upstream -- harmless here, and it
// gives the decoder's FENCE path dynamic coverage.

#define RVTEST_PASS                                                     \
        fence;                                                          \
        li TESTNUM, 1;                                                  \
        sw TESTNUM, tohost, t5;                                         \
        ebreak

// Upstream folds the failing test number into the exit word as
// (TESTNUM << 1) | 1 so the harness can report which case failed.
// Reproduced exactly; only the ecall exit is swapped for tohost+EBREAK.

#define RVTEST_FAIL                                                     \
        fence;                                                          \
        sll TESTNUM, TESTNUM, 1;                                        \
        or TESTNUM, TESTNUM, 1;                                         \
        sw TESTNUM, tohost, t5;                                         \
        ebreak

//-----------------------------------------------------------------------
// Data section macros
//-----------------------------------------------------------------------
// tohost/fromhost are defined here rather than in link.ld (which is
// how benchmarks/link.ld does it) because the vendored bodies expect
// the upstream layout. link.ld only places the .tohost section.

#define EXTRA_DATA

#define RVTEST_DATA_BEGIN                                               \
        EXTRA_DATA                                                      \
        .pushsection .tohost, "aw", @progbits;                          \
        .align 6; .global tohost; tohost: .dword 0;                     \
        .align 6; .global fromhost; fromhost: .dword 0;                 \
        .popsection;                                                    \
        .align 4; .global begin_signature; begin_signature:

#define RVTEST_DATA_END .align 4; .global end_signature; end_signature:

#endif  // _ENV_CPUSIM_RISCV_TEST_H
