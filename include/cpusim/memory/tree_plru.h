#pragma once
#include <cstdint>
#include <vector>
#include "cpusim/memory/eviction_policy.h"

namespace cpusim {

// TreePlru — tree-based pseudo-LRU replacement.
//
// Each set carries ways-1 internal-node bits arranged as a 1-based heap:
// node i has children 2i (left) and 2i+1 (right); leaf i maps to way
// i-ways. A bit steers 0=left, 1=right. Index 0 per set is unused.
//
//   victim(): walk root->leaf following the bits — the leaf is the way
//             to evict (the pseudo-oldest).
//   touch():  walk leaf->root flipping each node to point AWAY from the
//             accessed way, marking that path most-recently-used.
//
// ways must be a power of two so the tree is perfect.
class TreePlru : public EvictionPolicy {
public:
    TreePlru(uint32_t sets, unsigned ways);

    unsigned victim(uint32_t set) const     override;
    void     touch(uint32_t set, unsigned way) override;

private:
    unsigned             ways_;
    std::vector<uint8_t> bits_;   // sets * ways, 1-based heap per set
};

}  // namespace cpusim
