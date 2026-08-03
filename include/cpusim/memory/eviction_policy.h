#pragma once
#include <cstdint>

namespace cpusim {

// EvictionPolicy — the replacement policy of a set-associative cache,
// separated from the cache so it can be tested (and later swapped) on
// its own. The cache owns one and consults it only when a set is FULL:
//
//   victim(set):     which way to evict from a full set.
//   touch(set, way): record an access (hit or fill) so recency is fresh.
//
// Filling invalid ways first is the CACHE's job, not the policy's — the
// policy only orders recency among resident ways. Only tree-PLRU is
// implemented today (see TreePlru); this interface is the seam for a
// future SimConfig-selectable policy, mirroring bimodal vs gshare.
class EvictionPolicy {
public:
    virtual ~EvictionPolicy() = default;

    virtual unsigned victim(uint32_t set) const     = 0;
    virtual void     touch(uint32_t set, unsigned way) = 0;
};

}  // namespace cpusim
