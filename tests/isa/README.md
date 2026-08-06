# rv32ui — RISC-V ISA conformance tests

The official RV32I user-mode instruction tests, vendored from
[riscv-tests][rt]. Each test targets one instruction and checks it far
harder than a compiled C benchmark ever does: value corner cases,
sign-extension boundaries, `x0` as source and destination, and
explicit producer→consumer bypass sequences at 0, 1, and 2 nop
spacing.

That last group is why these matter here specifically — they walk the
forwarding paths described in [../../docs/hazard-forwarding.md](../../docs/hazard-forwarding.md)
one hazard distance at a time.

Before this suite the five C benchmarks left **16 of 39 RV32I
instructions with zero dynamic coverage**: `SLL` `SRL` `SRA` `SLT`
`SLTU` `SLTI` `SLTIU` `OR` `ORI` `LB` `LH` `LHU` `SH` `BLTU` `BGEU`
`FENCE`. The suite covers all of them.

## Running

```sh
tools/run_isa_tests.sh                    # whole suite
tools/run_isa_tests.sh build add sll      # named tests only
```

Requires `spike` and `riscv64-unknown-elf-gcc` on `PATH`, plus a built
`bench_run`. Each test is checked twice: Spike runs it standalone (do
its own assertions hold?), then [../../tools/spike_diff.sh](../../tools/spike_diff.sh)
compares our commit trace against Spike's line for line. The second
check is the one that tests the simulator.

Build artifacts and per-test logs land in `build/isa/`.

## Layout

```
tests/isa/
├── rv32ui/      vendored verbatim — thin shims, one per test
├── rv64ui/      vendored verbatim — the actual test bodies
├── macros/      vendored verbatim — test_macros.h
└── env/         OURS — replaces upstream riscv-test-env
```

Upstream splits every test in two: `rv32ui/add.S` only redefines
`RVTEST_RV64U` to `RVTEST_RV32U` and then includes `rv64ui/add.S`,
which holds the real test body and switches on `__riscv_xlen` for the
64-bit-only cases. Both directories are needed; `rv64ui` is the
upstream name, kept so re-syncing stays a plain copy.

`rv64ui/` holds only the files reachable from the rv32ui shims —
RV64-only tests (`addiw`, `sd`, `sraw`, …) are not vendored.

**Do not edit anything outside `env/`.** Those files are byte-identical
to upstream so that a re-sync is a copy, not a merge. All local
adaptation lives in `env/`.

## What `env/` replaces, and why

Upstream's `p` (physical, single-core) environment boots through
machine-mode CSRs: it installs an `mtvec` trap vector, programs
PMP and SATP, enters the test body via `mret`, and exits by trapping
on `ecall`. cpu-sim implements RV32I only — no CSRs, no traps, no
privilege modes — so none of that can run.

`env/riscv_test.h` provides the same `RVTEST_*` interface using
nothing but RV32I, which is what lets the vendored bodies assemble
unmodified. It mirrors [../../benchmarks/start.S](../../benchmarks/start.S):
zero the registers, set up a stack, run, then write the result to
`tohost` and halt with `EBREAK`. The exit encoding is unchanged from
upstream (`1` = pass, `(testnum << 1) | 1` = fail), so Spike still
reports which numbered case failed.

`env/link.ld` matches [../../benchmarks/link.ld](../../benchmarks/link.ld)
except that it does not define `tohost`/`fromhost` — the vendored
bodies emit those into a `.tohost` section themselves, as upstream
expects, so the script only places the section.

One non-obvious detail: `RVTEST_CODE_BEGIN` ends with an explicit
`j 1f`. The prologue assembles into `.text.init` and the test body
into `.text`, and alignment padding can leave a gap between the two
sections. Upstream crosses that gap with `mret`; falling through
instead executes the padding. This is not theoretical — it is exactly
what went wrong when this was first written, and it silently *worked*
for four tests whose sections happened to land adjacent.

## Excluded tests

Both are still vendored, just not run. `tools/run_isa_tests.sh` holds
the exclusion list; keep it in sync with this section.

| Test | Why |
|---|---|
| `fence_i` | Needs `FENCE.I` (Zifencei). cpu-sim decodes plain `FENCE` only, and models no I-cache that self-modifying code could go stale in. Does not assemble. |
| `ma_data` | Misaligned load/store. Spike traps misaligned accesses rather than emulating them, so the test needs the `mtvec` handler upstream installs. Ours is CSR-free, so the trap has nowhere to go and Spike spins at the first misaligned load — there is no usable reference trace, regardless of what cpu-sim does. |

Re-enabling `ma_data` needs a decision about misaligned semantics
first (emulate in the memory model, or trap — which means CSRs).

## Provenance

| Source | Commit | Files |
|---|---|---|
| [riscv-tests][rt] | `447a5fcb8253627ddb5f6a226f64e43463afcdd5` (2026-08-04) | `rv32ui/`, `rv64ui/`, `macros/test_macros.h` |
| [riscv-test-env][rte] | `a1c373ec89a3500630bafabf406108a8fc568bcc` (2026-01-09) | reference only — `env/` is a rewrite, not a copy |

Upstream license: `LICENSE.riscv-tests` (BSD 3-clause).

To re-sync, clone riscv-tests at the new commit and copy `rv32ui/*.S`,
the reachable `rv64ui/*.S`, and `macros/scalar/test_macros.h` over the
vendored trees. Leave `env/` alone, then re-run the suite — a diff in
behavior after a re-sync is upstream changing the environment
contract, which is the one thing that can break `env/`.

[rt]: https://github.com/riscv-software-src/riscv-tests
[rte]: https://github.com/riscv/riscv-test-env
