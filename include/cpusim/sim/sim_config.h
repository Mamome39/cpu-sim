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
    // Cycles for the data memory to serve one load/store. 1 = the
    // original single-cycle MEM; larger values stall the pipeline.
    unsigned dmem_latency_cycles = 1;
};

}  // namespace cpusim
