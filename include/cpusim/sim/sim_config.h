#pragma once
#include <cstddef>
#include <cstdint>

namespace cpusim {

// SimConfig — all tunable simulator parameters in one place.
//
// Passed to Core once at construction. Adding a new knob is a new
// field here; no constructor signature ever changes. Field names are
// spelled out (with units) so a config is self-documenting.
//
// Populate with plain field assignment and hand to Core:
//   SimConfig cfg;
//   cfg.dmem_latency_cycles = 5;
//   Core core(cfg);
struct SimConfig {
    // ── Memory layout ────────────────────────────────────────────
    uint32_t ram_base_addr  = 0x80000000;  // first address of RAM
    size_t   ram_size_bytes = 0x10000;     // RAM size (64 KiB)

    // ── Memory timing ────────────────────────────────────────────
    // Cycles for the backing data memory to serve one access. 1 = the
    // original single-cycle MEM. When the L1 cache is enabled this is
    // the miss penalty (cost of reaching the backing store).
    //
    // With the L2 enabled this becomes specifically the DRAM (L2-miss)
    // penalty, and l2_hit_latency_cycles is what an L1 miss costs when
    // the L2 hits. It then applies to BOTH sides: a unified L2 has one
    // DRAM under it, so imem_latency_cycles no longer describes what an
    // instruction miss pays past the L2. See the table in
    // docs/memory-model.md.
    unsigned dmem_latency_cycles = 1;

    // ── L1 data cache (direct-mapped, timing overlay) ────────────
    // When enabled, MEM talks to the cache instead of the backing
    // memory directly. Data is unaffected — the cache only models
    // hit/miss timing. sets and line_bytes must be powers of two.
    bool     dcache_enabled            = false;
    unsigned dcache_line_bytes         = 32;
    unsigned dcache_sets               = 64;   // 64 x 32 B = 2 KiB
    unsigned dcache_hit_latency_cycles = 1;
    unsigned dcache_ways               = 1;    // 1 = direct-mapped. Pow2.

    // ── Instruction memory + L1 I-cache (timing overlay) ──────────
    // Mirrors the D-cache: fetch talks to the cache instead of the
    // backing instruction memory directly when enabled. Read-only, so
    // the write-back/dirty machinery in Cache stays dormant here.
    unsigned imem_latency_cycles       = 1;    // I-miss penalty
    bool     icache_enabled            = false;
    unsigned icache_line_bytes         = 32;
    unsigned icache_sets               = 64;   // 64 x 32 B = 2 KiB
    unsigned icache_hit_latency_cycles = 1;
    unsigned icache_ways               = 1;    // 1 = direct-mapped. Pow2.

    // ── Instruction prefetch (I-cache only) ───────────────────────
    // When enabled, the I-cache runs a background "next-line" prefetch
    // on backing cycles the demand path leaves idle (serving a hit).
    // A demand access that catches an in-flight prefetch pays only the
    // remaining fetch time (partial hit) instead of a full miss. degree
    // is how many lines ahead to chain while the port stays idle.
    bool     icache_prefetch_enabled = false;
    unsigned icache_prefetch_degree  = 1;

    // ── L2 unified cache (timing overlay) ────────────────────────
    // One shared L2 sitting between BOTH L1s and DRAM: a single tag
    // array, so a line either L1 pulls in is already warm for the
    // other, and the two sides compete for the same capacity. Larger
    // and slower than an L1. Access cost by outcome:
    //
    //   L1 hit            the L1's own hit latency
    //   L1 miss, L2 hit   + l2_hit_latency_cycles
    //   L1 miss, L2 miss  + dmem_latency_cycles as well
    //
    // Enabling it with neither L1 on is allowed (it then behaves as a
    // single shared cache level), just unusual. sets, line_bytes and
    // ways must be powers of two.
    bool     l2_enabled            = false;
    unsigned l2_line_bytes         = 32;   // match the L1 line size
    unsigned l2_sets               = 512;  // 512 x 32 B = 16 KiB
    unsigned l2_hit_latency_cycles = 10;   // L1-miss cost when L2 hits
    unsigned l2_ways               = 1;    // 1 = direct-mapped. Pow2.

    // ── Branch predictor (BTB + bimodal 2-bit) ───────────────────
    // When enabled, IF predicts branch direction/target; a correct
    // prediction costs no bubble, a mispredict pays the usual 2-cycle
    // EX recovery. Off = always predict not-taken (the base behavior).
    // Entry counts must be powers of two.
    bool     bpred_enabled     = false;
    unsigned bpred_bht_entries = 256;   // 2-bit direction counters
    unsigned bpred_btb_entries = 64;    // target buffer entries
};

}  // namespace cpusim
