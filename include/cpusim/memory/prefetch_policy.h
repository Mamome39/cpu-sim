#pragma once
#include <cstdint>

namespace cpusim {

// PrefetchPolicy — decides WHICH line to prefetch next, separated from
// the Cache's own bookkeeping of WHEN the backing port is idle and
// how a demand access merges with an in-flight prefetch (see
// Cache::tick()). The cache owns one (only when prefetching is
// enabled) and consults it each time it needs a new target:
//
//   next(addr): the address of the line to prefetch after a demand
//               access (or a just-completed prefetch, when chaining up
//               to the configured degree) touched `addr`.
//
// Only SequentialPrefetch ("next-line", +1 line) is implemented today;
// this interface is the seam for a future policy (e.g. stride),
// mirroring EvictionPolicy vs TreePlru.
class PrefetchPolicy {
public:
    virtual ~PrefetchPolicy() = default;

    virtual uint32_t next(uint32_t addr) const = 0;
};

}  // namespace cpusim
