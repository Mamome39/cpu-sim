/* Minimal startup for cpu-sim benchmarks.
 * Sets up the stack from the linker-defined _stack_top symbol,
 * calls main(), then halts the simulator with EBREAK.
 * Section name .text.init matches the KEEP() entry in link.ld. */

.section .text.init, "ax"
.globl _start
_start:
    la   sp, _stack_top
    call main
    ebreak
