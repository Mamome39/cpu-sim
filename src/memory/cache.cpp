#include "cpusim/memory/cache.h"
#include "cpusim/memory/tree_plru.h"
#include <cassert>

namespace cpusim {

static unsigned log2_exact(unsigned v) {
    assert(v && (v & (v - 1)) == 0 && "must be a power of two");
    unsigned n = 0;
    while (v > 1) { v >>= 1; ++n; }
    return n;
}

Cache::Cache(IMemory& backing, unsigned line_bytes,
             unsigned sets, unsigned hit_latency, unsigned ways)
    : backing_(backing)
    , offset_bits_(log2_exact(line_bytes))
    , index_bits_(log2_exact(sets))
    , index_mask_(sets - 1)
    , hit_latency_(hit_latency ? hit_latency : 1)
    , ways_(ways)
    , lines_(static_cast<size_t>(sets) * ways)
    , repl_(std::make_unique<TreePlru>(sets, ways)) {}

uint32_t Cache::index_of(uint32_t addr) const {
    return (addr >> offset_bits_) & index_mask_;
}

uint32_t Cache::tag_of(uint32_t addr) const {
    return addr >> (offset_bits_ + index_bits_);
}

// Reconstruct a line's base address from its set and tag (for writeback).
uint32_t Cache::addr_of(uint32_t set, uint32_t tag) const {
    return (tag << (offset_bits_ + index_bits_)) | (set << offset_bits_);
}

Cache::Line& Cache::line_at(uint32_t set, unsigned way) {
    return lines_[static_cast<size_t>(set) * ways_ + way];
}
const Cache::Line& Cache::line_at(uint32_t set, unsigned way) const {
    return lines_[static_cast<size_t>(set) * ways_ + way];
}

int Cache::find_way(uint32_t addr) const {
    uint32_t set = index_of(addr);
    uint32_t t   = tag_of(addr);
    for (unsigned w = 0; w < ways_; ++w) {
        const Line& L = line_at(set, w);
        if (L.valid && L.tag == t) return static_cast<int>(w);
    }
    return -1;
}

unsigned Cache::choose_victim(uint32_t set) const {
    // Prefer an empty way; only consult the policy once the set is full.
    for (unsigned w = 0; w < ways_; ++w)
        if (!line_at(set, w).valid) return w;
    return repl_->victim(set);
}

// ── Data path — always served correctly by the backing memory ─────────
uint32_t Cache::load_word(uint32_t addr) const { return backing_.load_word(addr); }
uint16_t Cache::load_half(uint32_t addr) const { return backing_.load_half(addr); }
uint8_t  Cache::load_byte(uint32_t addr) const { return backing_.load_byte(addr); }
void Cache::store_word(uint32_t addr, uint32_t val) { backing_.store_word(addr, val); }
void Cache::store_half(uint32_t addr, uint16_t val) { backing_.store_half(addr, val); }
void Cache::store_byte(uint32_t addr, uint8_t  val) { backing_.store_byte(addr, val); }

// ── Timing path ───────────────────────────────────────────────────────
bool Cache::ready(uint32_t addr, bool is_write) {
    if (phase_ == Phase::Idle) {
        cur_addr_     = addr;
        cur_is_write_ = is_write;
        uint32_t set  = index_of(addr);
        int w = find_way(addr);
        last_hit_ = (w >= 0);
        if (last_hit_) {
            // Store hit dirties the line; no backing traffic.
            if (is_write) line_at(set, static_cast<unsigned>(w)).dirty = true;
            repl_->touch(set, static_cast<unsigned>(w));
            phase_         = Phase::HitWait;
            hit_remaining_ = static_cast<int>(hit_latency_) - 1;
        } else {
            // Pick the victim now so we know if a dirty flush is due.
            victim_way_ = choose_victim(set);
            const Line& v = line_at(set, victim_way_);
            if (v.valid && v.dirty) {
                wb_addr_ = addr_of(set, v.tag);
                phase_   = Phase::WriteBack;  // flush, then fetch
            } else {
                phase_ = Phase::MissFetch;    // fill happens in tick()
            }
        }
    }
    return phase_ == Phase::HitWait && hit_remaining_ == 0;
}

void Cache::tick() {
    switch (phase_) {
        case Phase::Idle:
            break;

        case Phase::HitWait:
            if (hit_remaining_ > 0) --hit_remaining_;
            else                    phase_ = Phase::Idle;  // served; release
            break;

        case Phase::WriteBack:
            // Flush the dirty victim to the backing (timing only — the
            // data already lives there under Option A). A backing that
            // is itself a cache takes the write and dirties its own line.
            if (backing_.ready(wb_addr_, /*is_write=*/true))
                phase_ = Phase::MissFetch;   // released next tick; then fetch
            backing_.tick();
            break;

        case Phase::MissFetch:
            // Drive the backing memory until it serves the line, then
            // install it into the chosen victim way and finish at hit
            // latency. A store miss is write-allocate → install dirty.
            if (backing_.ready(cur_addr_)) {
                uint32_t set = index_of(cur_addr_);
                Line& L  = line_at(set, victim_way_);
                L.valid  = true;
                L.dirty  = cur_is_write_;
                L.tag    = tag_of(cur_addr_);
                repl_->touch(set, victim_way_);
                phase_         = Phase::HitWait;
                hit_remaining_ = static_cast<int>(hit_latency_) - 1;
            }
            backing_.tick();
            break;
    }
}

}  // namespace cpusim
