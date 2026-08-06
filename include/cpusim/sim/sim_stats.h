#pragma once
#include <cstdint>

namespace cpusim {

// SimStats — a passive probe the pipeline writes performance events into.
//
// Each event is recorded at the stage where it is known: a branch
// mispredict at EX resolution (where actual vs. predicted is decided).
// A stage holds an optional SimStats* and records only when one is
// attached, so an un-probed run costs nothing and the datapath is
// unaffected either way.
//
// Grows as features land (cache hits/misses, stall causes, ...).
struct SimStats {
    // Clock cycles elapsed and instructions retired, counted by Core:
    // one cycle per tick(), one instruction per valid entry reaching
    // WB (stall/flush bubbles are not valid, so they do not count).
    // IPC = instructions / cycles.
    uint64_t cycles       = 0;
    uint64_t instructions = 0;

    // Control-flow predictions, counted at EX. The two sum to the total
    // dynamic branch/jump count, so accuracy = correct / (correct+mispr).
    uint64_t branch_correct     = 0;   // resolved == predicted
    uint64_t branch_mispredicts = 0;   // resolved != predicted

    // I-cache prefetch outcomes, counted by Cache. "Useful" = a demand
    // access later touched the prefetched line (hit or partial-hit
    // merge); "useless" = evicted before any demand ever used it.
    uint64_t icache_prefetch_useful  = 0;
    uint64_t icache_prefetch_useless = 0;
};

}  // namespace cpusim
