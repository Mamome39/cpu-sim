# Performance Report

Cycle counts for the RV32I 5-stage in-order core across memory
configurations. Data is a timing-model result — every configuration
produces **byte-identical committed results** (the Spike diff holds),
so only cycle counts move.

Generated with `tools/bench_run`; instruction counts are retired
commit-trace lines. Reproduce steps are at the bottom.

---

## Configurations

| Name | Flags | Meaning |
|------|-------|---------|
| **ideal** | `--mem-latency=1` | Single-cycle memory — the pipeline's floor (memory is free; only hazards and branch flushes cost cycles). |
| **slow** | `--mem-latency=5` | 5-cycle data memory, no cache — a realistic slow backing store. |
| **cache** | `--mem-latency=5 --dcache` | Same slow memory behind an L1 D-cache (5-cycle access is now the **miss penalty**). |

L1 D-cache: direct-mapped, 64 sets × 32 B line = **2 KiB**, 1-cycle hit,
write-allocate, timing-only overlay.

---

## Results

Cycles (lower is better), with IPC = instructions / cycles:

| Benchmark | Instrs | ideal | IPC | slow | IPC | cache | IPC |
|-----------|-------:|------:|----:|-----:|----:|------:|----:|
| bubble_sort | 16,653 | 23,091 | 0.72 | 55,611 | 0.30 | 23,136 | 0.72 |
| fibonacci | 2,100,697 | 2,850,951 | 0.74 | 5,851,939 | 0.36 | 2,851,021 | 0.74 |
| crc32 | 16,324 | 20,553 | 0.79 | 21,925 | 0.74 | 20,733 | 0.79 |
| matmul | 442,398 | 603,877 | 0.73 | 639,821 | 0.69 | 606,702 | 0.73 |
| sieve | 32,015 | 45,983 | 0.70 | 70,575 | 0.45 | 46,303 | 0.69 |

Derived ratios:

| Benchmark | slow ÷ ideal (cost of slow memory) | slow ÷ cache (cache speedup) | cache ÷ ideal (gap to floor) |
|-----------|:---:|:---:|:---:|
| bubble_sort | 2.41× | **2.40×** | 1.002× |
| fibonacci | 2.05× | **2.05×** | 1.00002× |
| crc32 | 1.07× | 1.06× | 1.009× |
| matmul | 1.06× | 1.05× | 1.005× |
| sieve | 1.53× | **1.52×** | 1.007× |

---

## Observations

- **The cache recovers essentially all of the slow-memory penalty.** The
  `cache` column lands within a fraction of a percent of `ideal`
  (fibonacci: 0.002%, bubble_sort: 0.2%). The residual gap is cold-miss
  overhead — the first touch of each line still pays the miss penalty.

- **Why it recovers so completely:** every benchmark's working set fits
  in the 2 KiB cache, so after warm-up almost every access hits. A
  benchmark whose footprint exceeded the cache would show it helping
  less — that is exactly what a larger benchmark (or a smaller cache
  config) would expose next.

- **Memory-bound vs compute-bound is visible in the spread.**
  bubble_sort and fibonacci are dominated by loads/stores (tight loops,
  stack traffic), so slow memory hurts ~2× and the cache matters most.
  crc32 and matmul are compute-bound — crc32 is ALU-heavy, matmul spends
  most cycles in the software `__mulsi3` routine — so memory latency
  barely registers either way (~1.05×).

- **matmul is the interesting near-miss.** Its inner loop walks a column
  of `b` with a 64-byte stride against a 32-byte line, so `b` misses on
  nearly every access — but because multiplies dominate the cycle count,
  even that poor locality moves the total only ~5%.

- **IPC ceiling is below 1.0 even at `ideal`** (~0.7–0.8). With no branch
  predictor yet, every taken branch costs a 2-cycle flush, and load-use
  hazards add stalls. Closing that gap is the branch-predictor stage.

---

## Reproduce

```sh
cmake -S . -B build
cmake --build build -j$(nproc)          # builds the core + benchmarks

# For each benchmark <b> in bubble_sort fibonacci crc32 matmul sieve:
./build/bench_run --mem-latency=1          ./build/<b>.elf   # ideal
./build/bench_run --mem-latency=5          ./build/<b>.elf   # slow
./build/bench_run --mem-latency=5 --dcache ./build/<b>.elf   # cache

# Instruction count (retired commit-trace lines), config-invariant:
./build/bench_run --trace=/tmp/t.trace ./build/<b>.elf && wc -l /tmp/t.trace
```

`bench_run` prints `cycles:` in its summary. Correctness of any config is
checked with `tools/spike_diff.sh ./build/<b>.elf ./build` (data is
independent of the timing configuration).
