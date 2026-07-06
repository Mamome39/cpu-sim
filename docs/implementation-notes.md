# Implementation Notes

Design decisions and tradeoffs behind ELF loading, commit tracing, and
Spike diff-testing — each stated as *what was done, and why*.

---

## ELF Loader

Every `PT_LOAD` segment is loaded into **both** `imem_` and `dmem_` because
the Core uses a Harvard model (separate instruction and data memories) while
an ELF assumes a single flat address space. Instruction fetches always go to
`imem_` and data accesses always go to `dmem_`; without duplicating the load,
a data instruction like `lw` would miss in `dmem_` even though the bytes
exist in `imem_`.

The ELF32 structs are defined inline in `elf_loader.cpp` rather than pulled
from `<elf.h>` because that header is Linux-only and macOS does not ship it.

The BSS region (`memsz − filesz` bytes) is zeroed in `dmem_` because BSS
sections have a larger memory size than their file size: the file carries no
bytes for them, but the C runtime expects them zeroed.

The entry point is checked against `Core::base_` and a mismatch throws,
rather than silently relocating the PC, because a wrong entry point would
otherwise produce incorrect execution with no obvious error.

---

## Benchmark Startup (`start.S`)

A `1` is written to the `tohost` symbol before `ebreak` because Spike and
cpu-sim halt on different signals. Spike polls `tohost` as a special MMIO
address and exits when it reads non-zero — `ebreak` alone leaves Spike
looping in M-mode with no handler. cpu-sim, in turn, halts when `ebreak`
reaches WB. The two instructions sit back-to-back so both simulators stop at
the same point in program order.

---

## Tracer Format

Register writes are printed in `x<N>` notation rather than ABI names
(`ra`, `sp`) because that is what Spike's `--log-commits` emits. With
mismatched notation a `diff` would flag every register-write line as
different regardless of whether the values agreed.

The register number is formatted with `x%-2d` (left-justified in a 2-char
field) because Spike aligns the same way: `x1 ` for single digits, `x10` for
two. The field width must match exactly for the diff to stay clean.

Load addresses are captured in `wb.mem_addr` and emitted as ` mem 0x<addr>`
after the register value because Spike does the same. Dropping them would
make every load line differ from Spike's even when the loaded value is right.

Store values are printed at their access width — `SB` → 2 hex digits,
`SH` → 4, `SW` → 8 — because Spike logs `mem 0x... 0x01` for a byte store,
not `0x00000001`. A hardcoded 8-digit width passed on word-only benchmarks
but diverged the moment the sieve used `sb`.

---

## bench_run (`tools/bench_run.cpp`)

The error message in `Core::load_elf()` is built with a plain `snprintf`
buffer rather than a lambda because clangd loses member-variable scope inside
a lambda body placed in a constructor initializer — every later reference to
`base_` or `imem_` was flagged as undefined, making the whole file look broken
in the IDE.

The `Tracer` is held in a `std::unique_ptr` when `--trace=<file>` is given so
it outlives `core.run()`. A stack-allocated Tracer inside the `if` block would
be destroyed before `run()` ever saw an instruction.

---

## Spike Diff Script (`tools/spike_diff.sh`)

Both traces are filtered to `PC ≥ base` (0x80000000) and the trailing
`ebreak` line is stripped, rather than filtering to `PC ≥ main`. Spike's MROM
boot preamble lives below `base`, so the base filter drops it; the only other
divergence is the exit — Spike halts on the `tohost` store one instruction
before cpu-sim commits `ebreak`, so removing that single line aligns the
tails. An earlier version filtered to `main`'s address (via `nm`), but under
`-O1` the compiler can place helper functions *below* `main`: fibonacci's
`fib` landed there, so the whole recursion fell outside the window and the
diff "passed" on seven lines. Filtering by `base` is location-independent.

A portable inline hex-to-decimal function is used in awk instead of
`strtonum()` because macOS ships BSD awk (nawk), which lacks that gawk
extension; without it the PC comparison would fail silently on every Mac.

The bench_run path is tested with `[ -x "$BENCH_RUN" ]` rather than
`command -v "$BENCH_RUN"` because `command -v` resolves names in `$PATH`, and
a relative path like `./build/bench_run` is not a `$PATH` entry — the check
failed regardless of whether the binary existed.

The `.trace` extension is placed *inside* the `mktemp` `X`-block template
(`/tmp/cpusim_ours_full_XXXXXX.trace`) because macOS `mktemp` treats anything
after the last `X` as a fixed literal suffix and can collide across runs,
raising `mktemp: mkstemp failed: File exists` on the second invocation.
