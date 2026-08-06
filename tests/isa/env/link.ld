/* Linker script for the vendored rv32ui ISA tests.
 *
 * Same load base and overall shape as benchmarks/link.ld, with one
 * difference: tohost/fromhost are NOT defined here. The vendored test
 * bodies emit them into a .tohost section via RVTEST_DATA_BEGIN (see
 * env/riscv_test.h), matching upstream, so this script only places
 * that section. */

OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x80000000;

    .text.init : { *(.text.init) }
    .text      : { *(.text*) }
    .rodata    : { *(.rodata*) }

    . = ALIGN(0x1000);
    .data      : { *(.data*) }

    /* Defined by the test body, not here. Spike polls this symbol to
       detect exit: 1 = pass, (testnum << 1) | 1 = fail. */
    . = ALIGN(0x40);
    .tohost    : { *(.tohost) }

    . = ALIGN(0x1000);
    .bss       : { *(.bss*) }

    . = ALIGN(0x1000);
    _stack_top = . + 0x4000;    /* 16 KiB stack */
}
