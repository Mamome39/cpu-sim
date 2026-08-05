# Performance Report

Cycle counts for the RV32I 5-stage in-order core across memory and
branch-prediction configurations. Every configuration produces
**byte-identical committed results** (the Spike diff holds), so only
cycle counts move — this is a pure timing study.

Three independent levers attack three different cycle sources:
- the **L1 D-cache** removes data memory-stall cycles (miss penalty),
- the **L1 I-cache** removes instruction-fetch-stall cycles (same idea,
  applied to the front end),
- the **branch predictor** removes branch-flush cycles (each
  misprediction is a 2-cycle flush in this pipeline).

Generated with `tools/bench_run`; instruction counts are retired
commit-trace lines. Reproduce steps are at the bottom.

---

## Configurations

| Name | Flags | Meaning |
|------|-------|---------|
| **ideal** | `--mem-latency=1` | Single-cycle memory — the pipeline floor. |
| **slow** | `--mem-latency=5` | 5-cycle memory, no cache. |
| **cache** | `--mem-latency=5 --dcache` | Slow memory behind the L1 D-cache (5 = miss penalty). |
| **islow** | `--imem-latency=5` | 5-cycle instruction memory, no I-cache. |
| **icache** | `--imem-latency=5 --icache` | Slow instruction memory behind the L1 I-cache. |
| **bpred** | `... --bpred` | Adds the BTB + bimodal branch predictor. |

L1 D-cache and I-cache: direct-mapped, 64 sets × 32 B = 2 KiB, 1-cycle
hit (same default geometry for both; I-cache is read-only, so its
dirty/write-back machinery — shared with the D-cache's `Cache` class —
stays dormant). Branch predictor: BTB (64 entries) + 256 × 2-bit
counters, off by default.

---

## Memory (cache)

Cycles, with IPC = instructions / cycles. Predictor off throughout.

| Benchmark | Instrs | ideal | IPC | slow | IPC | cache | IPC |
|-----------|-------:|------:|----:|-----:|----:|------:|----:|
| bubble_sort | 16,653 | 23,091 | 0.72 | 55,611 | 0.30 | 23,136 | 0.72 |
| fibonacci | 2,100,697 | 2,850,951 | 0.74 | 5,851,939 | 0.36 | 2,851,021 | 0.74 |
| crc32 | 16,324 | 20,553 | 0.79 | 21,925 | 0.74 | 20,733 | 0.79 |
| matmul | 442,398 | 603,877 | 0.73 | 639,821 | 0.69 | 606,702 | 0.73 |
| sieve | 32,015 | 45,983 | 0.70 | 70,575 | 0.45 | 46,303 | 0.69 |

The cache recovers essentially all of the slow-memory penalty — `cache`
lands within a fraction of a percent of `ideal` (working sets fit in
2 KiB). Compute-bound crc32/matmul barely move either way.

---

## Instruction cache

Same experiment on the fetch side: hold data memory ideal (`--mem-latency=1`,
no D-cache) and vary the instruction side. Fetch is now a real
`ready()/tick()` client — an I-miss stalls the front end (IF/ID takes a
bubble, PC holds) while everything already past IF keeps draining.

| Benchmark | Instrs | ideal | IPC | islow | IPC | icache | IPC |
|-----------|-------:|------:|----:|------:|----:|-------:|----:|
| bubble_sort | 16,653 | 23,091 | 0.72 | 83,269 | 0.20 | 23,115 | 0.72 |
| fibonacci | 2,100,697 | 2,850,951 | 0.74 | 10,503,489 | 0.20 | 2,850,975 | 0.74 |
| crc32 | 16,324 | 20,553 | 0.79 | 81,624 | 0.20 | 20,581 | 0.79 |
| matmul | 442,398 | 603,877 | 0.73 | 2,211,994 | 0.20 | 603,941 | 0.73 |
| sieve | 32,015 | 45,983 | 0.70 | 160,079 | 0.20 | 46,008 | 0.70 |

Two things stand out:

- **`islow` IPC is exactly 0.20 on every benchmark, regardless of
  program shape.** With no I-cache, *every* fetch pays the full 5-cycle
  instruction-memory latency and the front-end stall never overlaps
  itself — so throughput hits a hard ceiling of 1 instruction per
  `imem_latency` cycles (1/5 = 0.20). This is a clean sanity check that
  the front-end stall model is charging exactly what it should, no
  more.
- **The I-cache erases nearly all of it**, landing within ~0.1% of
  `ideal` on every benchmark — closer than the D-cache did. Code is
  overwhelmingly sequential, so a 32 B line (8 instructions) is almost
  always fully consumed before the next miss: one miss buys 8 back-to-back
  hits, versus the more scattered access pattern on the data side.

---

## Branch prediction

Isolated at single-cycle memory (no cache), so branch flushes are the
only variable. `mispr` = mispredictions (= 2-cycle flushes).

| Benchmark | base cyc | mispr | IPC | bpred cyc | mispr | IPC | cyc saved |
|-----------|---------:|------:|----:|----------:|------:|----:|:---------:|
| bubble_sort | 23,091 | 2,210 | 0.72 | 18,815 | 72 | **0.89** | 18.5% |
| fibonacci | 2,850,951 | 375,126 | 0.74 | 2,550,855 | 225,078 | **0.82** | 10.5% |
| crc32 | 20,553 | 2,092 | 0.79 | 16,897 | 264 | **0.97** | 17.8% |
| matmul | 603,877 | 80,738 | 0.73 | 472,855 | 15,227 | **0.94** | 21.7% |
| sieve | 45,983 | 5,984 | 0.70 | 34,883 | 434 | **0.92** | 24.1% |

The penalty is exactly **2 cycles per misprediction**, so the saving is
`2 × (mispredicts eliminated)`. For bubble_sort:
`(2210 − 72) × 2 = 4276 = 23091 − 18815`, exactly.

fibonacci gains least: it is call/return-heavy, and `ret` is a **JALR**
(register target) which the predictor deliberately leaves unpredicted —
its residual mispredicts are mostly returns. That is the textbook
motivation for a **return-address stack**.

---

## Both levers together

Starting from realistic slow memory and adding each lever:

| Benchmark | slow | + cache | + cache + bpred | vs ideal+bpred floor |
|-----------|-----:|--------:|----------------:|:--------------------:|
| bubble_sort | 55,611 | 23,136 | 18,860 | +0.2% |
| fibonacci | 5,851,939 | 2,851,021 | 2,550,925 | +0.003% |
| crc32 | 21,925 | 20,733 | 17,077 | +1.1% |
| matmul | 639,821 | 606,702 | 475,680 | +0.6% |
| sieve | 70,575 | 46,303 | 35,203 | +0.9% |

Because the two levers target independent cycle sources, they stack: the
cache brings a slow machine back to ideal-memory cycles, and the
predictor then removes the branch flushes — landing within ~1% of the
`ideal + bpred` floor (free memory + prediction). Neither lever helps the
part the other one owns.

---

## All three levers: D-cache + I-cache + branch predictor

Both memories realistically slow (`--mem-latency=5 --imem-latency=5`),
both caches on, predictor on — versus the same machine with no caches
and no predictor at all, and versus the `ideal + bpred` floor from the
table above (single-cycle memory on both sides, predictor on).

| Benchmark | no caches, no bpred | + D+I cache | + bpred | vs ideal+bpred floor |
|-----------|---------------------:|------------:|--------:|:---------------------:|
| bubble_sort | 115,789 | 23,165 | 18,889 | +0.4% |
| fibonacci | 13,504,477 | 2,851,045 | 2,550,949 | +0.004% |
| crc32 | 82,996 | 20,781 | 17,125 | +1.3% |
| matmul | 2,247,938 | 608,146 | 477,124 | +0.9% |
| sieve | 184,671 | 46,333 | 35,233 | +1.0% |

Adding the I-cache into the mix costs a hair more than D-cache-only +
bpred did (compare to the `vs ideal+bpred floor` column two sections
up) — an extra cold-start miss per instruction line that the D-cache
comparison didn't pay. Still within ~1.3% of the fully-ideal floor: two
independent, realistically-sized caches plus a predictor recover
essentially all the stall/flush overhead a slow, unpredicted machine
pays.

---

## Observations

- **D-cache** matters where memory access dominates with good locality
  (bubble_sort, fibonacci ~2× under slow memory); little where compute
  dominates (crc32, matmul) or the access pattern thrashes (matmul's
  column walk).
- **I-cache** matters everywhere, more uniformly than the D-cache —
  every benchmark is fetch-bound without it (flat 0.20 IPC ceiling) and
  every benchmark recovers to ~ideal with it, because instruction streams
  are far more sequential than data access patterns.
- **Prediction** matters everywhere — every benchmark is branch-heavy —
  lifting IPC from ~0.72 to ~0.82–0.97. Mispredicts collapse (bubble_sort
  2210 → 72).
- **IPC still peaks below 1.0** even with both levers: remaining losses
  are load-use stalls and the unpredicted JALR returns. A return-address
  stack and (someday) out-of-order execution are the next levers.

---

## Reproduce

```sh
cmake -S . -B build
cmake --build build -j$(nproc)          # builds the core + benchmarks

# For each benchmark <b> in bubble_sort fibonacci crc32 matmul sieve:
./build/bench_run --mem-latency=1                  ./build/<b>.elf  # ideal
./build/bench_run --mem-latency=1 --bpred          ./build/<b>.elf  # + predictor
./build/bench_run --mem-latency=5                  ./build/<b>.elf  # slow
./build/bench_run --mem-latency=5 --dcache         ./build/<b>.elf  # + cache
./build/bench_run --mem-latency=5 --dcache --bpred ./build/<b>.elf  # both

# Instruction-side (D-side held ideal at --mem-latency=1):
./build/bench_run --mem-latency=1 --imem-latency=5           ./build/<b>.elf  # islow
./build/bench_run --mem-latency=1 --imem-latency=5 --icache  ./build/<b>.elf  # + I-cache

# All three levers together:
./build/bench_run --mem-latency=5 --imem-latency=5                             ./build/<b>.elf
./build/bench_run --mem-latency=5 --imem-latency=5 --dcache --icache           ./build/<b>.elf
./build/bench_run --mem-latency=5 --imem-latency=5 --dcache --icache --bpred   ./build/<b>.elf

# bench_run prints cycles: and mispred: in its summary.
# Instruction count (retired commit-trace lines), config-invariant:
./build/bench_run --trace=/tmp/t.trace ./build/<b>.elf && wc -l /tmp/t.trace
```

Correctness of any config: `tools/spike_diff.sh ./build/<b>.elf ./build`
(data is independent of the timing configuration).
