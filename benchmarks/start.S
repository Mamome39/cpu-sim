/* Minimal startup for cpu-sim benchmarks.
 * Sets up the stack, calls main(), then signals Spike via the
 * tohost symbol and halts cpu-sim with EBREAK.
 * Section .text.init matches the KEEP() entry in link.ld. */

.section .text.init, "ax"
.globl _start
_start:
    la   sp, _stack_top
    call main
    /* Write 1 to tohost — Spike polls this and exits cleanly. */
    la   t0, tohost
    li   t1, 1
    sw   t1, 0(t0)
    /* EBREAK halts cpu-sim. */
    ebreak
1:  j    1b
