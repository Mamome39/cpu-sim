#include "cpusim/memory/tree_plru.h"
#include <cassert>

namespace cpusim {

TreePlru::TreePlru(uint32_t sets, unsigned ways)
    : ways_(ways)
    , bits_(static_cast<size_t>(sets) * ways, 0) {
    assert(ways && (ways & (ways - 1)) == 0 &&
           "ways must be a power of two");
}

unsigned TreePlru::victim(uint32_t set) const {
    size_t   base = static_cast<size_t>(set) * ways_;
    unsigned i = 1;
    while (i < ways_) i = 2 * i + bits_[base + i];
    return i - ways_;
}

void TreePlru::touch(uint32_t set, unsigned way) {
    size_t   base = static_cast<size_t>(set) * ways_;
    unsigned i = way + ways_;          // leaf index for `way`
    while (i > 1) {
        unsigned parent = i / 2;
        // Steer future victims AWAY from the way just accessed.
        bits_[base + parent] = (i == 2 * parent) ? 1 : 0;
        i = parent;
    }
}

}  // namespace cpusim
