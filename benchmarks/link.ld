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

    /* Spike polls the 'tohost' symbol to detect program exit.
       Writing 1 = pass, (code<<1)|1 = fail with exit code. */
    . = ALIGN(0x40);
    .tohost : {
        tohost   = .; LONG(0); LONG(0);
        fromhost = .; LONG(0); LONG(0);
    }

    . = ALIGN(0x1000);
    .bss : { *(.bss*) }

    . = ALIGN(0x1000);
    _stack_top = . + 0x4000;    /* 16 KiB stack */
}
