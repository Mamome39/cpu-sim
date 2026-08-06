#pragma once
#include "cpusim/memory/prefetch_policy.h"

namespace cpusim {

// SequentialPrefetch — the "next-line" policy: always prefetch the
// line immediately following the one just accessed. Sound for
// instruction streams, overwhelmingly sequential between branches.
class SequentialPrefetch : public PrefetchPolicy {
public:
    explicit SequentialPrefetch(unsigned line_bytes) : line_bytes_(line_bytes) {}

    uint32_t next(uint32_t addr) const override { return addr + line_bytes_; }

private:
    unsigned line_bytes_;
};

}  // namespace cpusim
