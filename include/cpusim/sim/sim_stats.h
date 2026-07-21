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
    uint64_t branch_mispredicts = 0;
};

}  // namespace cpusim
