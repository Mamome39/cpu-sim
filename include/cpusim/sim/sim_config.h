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
