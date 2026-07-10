#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "cpusim/memory/mem_interface.h"

namespace cpusim {

// Cache — a direct-mapped, timing-only L1 overlay over a backing
// IMemory (e.g. FlatMem as "DRAM", or a lower cache level).
//
// Data is never stored here: load_*/store_* delegate to the backing
// memory, so values are always correct and immediate. The cache holds
// only tags, and its job is purely to model access TIMING:
//
//   ready(addr): a HIT is served in hit_latency cycles; a MISS drives
//                the backing memory's ready()/tick() (recursively, so
//                an L2 composes for free), then installs the line and
//                serves at hit_latency. One blocking request at a time,
//                matching the in-order MEM stage.
//   tick():      advances the timing model (and the backing on a miss).
//
// v1 limitations: direct-mapped (no replacement policy) and no dirty-
// eviction writeback penalty. Both are planned refinements.
class Cache : public IMemory {
public:
    // line_bytes and sets must be powers of two.
    Cache(IMemory& backing,
          unsigned line_bytes,
          unsigned sets,
          unsigned hit_latency);

    // Data path — delegated to the backing memory (always correct).
    uint32_t load_word(uint32_t addr) const override;
    uint16_t load_half(uint32_t addr) const override;
    uint8_t  load_byte(uint32_t addr) const override;
    void store_word(uint32_t addr, uint32_t val) override;
    void store_half(uint32_t addr, uint16_t val) override;
    void store_byte(uint32_t addr, uint8_t  val) override;

    // Timing path.
    bool ready(uint32_t addr) override;
    void tick() override;

    // Inspection (for tests): was the current/last request a hit?
    bool last_was_hit() const { return last_hit_; }

private:
    enum class Phase { Idle, HitWait, MissFetch };

    uint32_t index_of(uint32_t addr) const;
    uint32_t tag_of(uint32_t addr)   const;
    bool     is_hit(uint32_t addr)   const;

    IMemory& backing_;

    unsigned offset_bits_;   // log2(line_bytes)
    unsigned index_bits_;    // log2(sets)
    uint32_t index_mask_;    // sets - 1
    unsigned hit_latency_;

    std::vector<bool>     valid_;
    std::vector<uint32_t> tag_;

    // Timing state — one outstanding request.
    Phase    phase_        = Phase::Idle;
    uint32_t cur_addr_     = 0;
    int      hit_remaining_ = 0;
    bool     last_hit_     = false;
};

}  // namespace cpusim
