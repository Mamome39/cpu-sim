# Implementation Notes

Design decisions and tradeoffs encountered while building ELF loading,
commit tracing, and Spike diff-testing. 

---

## ELF Loader

I load every `PT_LOAD` segment into **both** `imem_` and `dmem_` because
the Core uses a Harvard model (separate instruction and data memories) while
ELF assumes a single flat address space. Instruction fetches always go to
`imem_`; data accesses always go to `dmem_`. Without duplicating the load,
a data instruction like `lw` would miss in `dmem_` even though the bytes
exist in `imem_`.

I define ELF32 structs inline in `elf_loader.cpp` instead of including
`<elf.h>` because macOS does not ship that header — it is Linux-only.

I zero `memsz − filesz` bytes in `dmem_` for each segment because BSS
sections have a larger memory size than their file size: the file contains
no bytes for BSS, but the C runtime expects them zeroed.

I enforce `e_entry == Core::base_` in `Core::load_elf()` and throw rather
than silently misplace the PC, because a wrong entry point would produce
incorrect execution with no obvious error.

---

## Benchmark Startup (`start.S`)

I write 1 to the `tohost` symbol before `ebreak` because Spike and cpu-sim
need different halt signals. Spike polls `tohost` as a special MMIO address
and exits when it reads non-zero — `ebreak` alone leaves Spike looping in
M-mode with no handler. cpu-sim detects `ebreak` at WB and sets `halted_`.
The two instructions live back-to-back so both simulators always halt at
the same point in program order.

---

## Tracer Format

I changed register output from ABI names (`ra`, `sp`) to `x<N>` notation
(`x1`, `x2`) because that is what Spike's `--log-commits` produces. Without
matching notation, a `diff` would flag every register-write line as different
regardless of whether the values were correct.

I use `x%-2d` (left-justify in a 2-char field) because Spike formats
single-digit registers as `x1 ` and two-digit registers as `x10` — the
same field width. This alignment must match exactly for the diff to be clean.

I record `wb.mem_addr` for load instructions and emit it as
` mem 0x<addr>` after the register value because Spike does the same. Omitting
load addresses would cause every load line to differ from Spike's trace even
when the loaded value is correct.

I print a store's value at its access width (`SB` → 2 hex digits, `SH` → 4,
`SW` → 8) because Spike does — it logs `mem 0x... 0x01` for a byte store, not
`0x00000001`. Hardcoding 8 digits passed on word-only benchmarks but diverged
the moment a benchmark (the sieve) used `sb`.

---

## bench_run (`tools/bench_run.cpp`)

I use a plain `snprintf` char buffer for the error message in
`Core::load_elf()` instead of a lambda because clangd loses member-variable
scope inside a lambda body inside a constructor initializer — every subsequent
reference to `base_` or `imem_` was flagged as undefined, making the entire
file appear broken in the IDE.

I hold the `Tracer` in a `std::unique_ptr` when `--trace=<file>` is given
so the tracer outlives `core.run()`. A stack-allocated Tracer inside an `if`
block would be destroyed before `run()` sees any instructions.

---

## Spike Diff Script (`tools/spike_diff.sh`)

I filter both traces to `PC ≥ base` (0x80000000) and strip the trailing
`ebreak` line, rather than filtering to `PC ≥ main`. The MROM boot preamble
Spike emits lives below `base`, so the base filter drops it; the only other
divergence is the exit — Spike halts on the `tohost` store one instruction
before cpu-sim commits `ebreak`, so removing that one line aligns them. An
earlier version filtered to `main`'s address (via `nm`), but with `-O1` the
compiler can place helper functions *below* `main` — fibonacci's `fib` landed
before `main`, so the whole recursion was silently filtered out and the diff
"passed" on seven lines. Filtering by `base` is location-independent.

I write a portable inline hex-to-decimal function in awk instead of using
`strtonum()` because macOS ships BSD awk (nawk), which does not implement
that gawk extension. Without a portable converter the PC comparison would
fail silently on every Mac.

I check `[ -x "$BENCH_RUN" ]` instead of `command -v "$BENCH_RUN"` for the
bench_run path because `command -v` resolves names in `$PATH` — a relative
path like `./build/bench_run` is not a `$PATH` entry and the check always
failed regardless of whether the binary existed.

I put the `.trace` extension *inside* the `mktemp` `X`-block template
(`/tmp/cpusim_ours_full_XXXXXX.trace`) because macOS `mktemp` treats
anything after the last `X` as a fixed literal suffix and can collide across
runs, causing `mktemp: mkstemp failed: File exists` on the second invocation.
