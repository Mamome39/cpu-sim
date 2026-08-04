#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "cpusim/memory/mem_interface.h"
#include "cpusim/memory/eviction_policy.h"

namespace cpusim {

// Cache — a set-associative, timing-only L1 overlay over a backing
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
// Organization: `sets` sets of `ways` lines each (ways == 1 is plain
// direct-mapped). On a miss the victim is the first invalid way, else
// the tree-Pseudo-LRU victim (ways-1 bits per set).
//
// Write-back: a store is served at hit latency and only marks its line
// dirty (no backing traffic per store). The writeback cost is paid later,
// when a dirty line is evicted — the miss first drives the backing to
// flush the victim, then fetches the new line. Data still lives in the
// backing (Option A), so the flush is timing-only but honestly charged.
class Cache : public IMemory {
public:
    // line_bytes, sets, and ways must be powers of two. ways defaults to
    // 1 (direct-mapped) so existing call sites are unaffected.
    Cache(IMemory& backing,
          unsigned line_bytes,
          unsigned sets,
          unsigned hit_latency,
          unsigned ways = 1);

    // Data path — delegated to the backing memory (always correct).
    uint32_t load_word(uint32_t addr) const override;
    uint16_t load_half(uint32_t addr) const override;
    uint8_t  load_byte(uint32_t addr) const override;
    void store_word(uint32_t addr, uint32_t val) override;
    void store_half(uint32_t addr, uint16_t val) override;
    void store_byte(uint32_t addr, uint8_t  val) override;

    // Timing path.
    bool ready(uint32_t addr, bool is_write) override;
    void tick() override;

    // Inspection (for tests): was the current/last request a hit?
    bool last_was_hit() const { return last_hit_; }

private:
    enum class Phase { Idle, HitWait, WriteBack, MissFetch };

    // One cache line's tag state (no data — see the class comment).
    struct Line {
        bool     valid = false;
        bool     dirty = false;   // reserved for write-back (Step 2)
        uint32_t tag   = 0;
    };

    uint32_t index_of(uint32_t addr) const;
    uint32_t tag_of(uint32_t addr)   const;

    // Return the way holding addr in its set, or -1 on a miss.
    int  find_way(uint32_t addr) const;
    // Pick the way to install into: first invalid, else the policy's
    // victim (only consulted once the set is full).
    unsigned choose_victim(uint32_t set) const;

    Line&       line_at(uint32_t set, unsigned way);
    const Line& line_at(uint32_t set, unsigned way) const;

    IMemory& backing_;

    unsigned offset_bits_;   // log2(line_bytes)
    unsigned index_bits_;    // log2(sets)
    uint32_t index_mask_;    // sets - 1
    unsigned hit_latency_;
    unsigned ways_;

    std::vector<Line> lines_;      // sets * ways, row-major by set
    std::unique_ptr<EvictionPolicy> repl_;   // replacement policy

    // Address of the line resident in (set, way) — for victim writeback.
    uint32_t addr_of(uint32_t set, uint32_t tag) const;

    // Timing state — one outstanding request.
    Phase    phase_         = Phase::Idle;
    uint32_t cur_addr_      = 0;
    bool     cur_is_write_  = false;   // is the current access a store?
    unsigned victim_way_    = 0;       // way chosen at miss start
    uint32_t wb_addr_       = 0;       // dirty victim's address to flush
    int      hit_remaining_ = 0;
    bool     last_hit_      = false;
};

}  // namespace cpusim
