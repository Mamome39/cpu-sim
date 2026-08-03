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
bool Cache::ready(uint32_t addr) {
    if (phase_ == Phase::Idle) {
        cur_addr_ = addr;
        int w = find_way(addr);
        last_hit_ = (w >= 0);
        if (last_hit_) {
            repl_->touch(index_of(addr), static_cast<unsigned>(w));
            phase_         = Phase::HitWait;
            hit_remaining_ = static_cast<int>(hit_latency_) - 1;
        } else {
            phase_ = Phase::MissFetch;   // fill happens in tick()
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

        case Phase::MissFetch:
            // Drive the backing memory until it serves the line, then
            // install it into a victim way and finish at hit latency.
            if (backing_.ready(cur_addr_)) {
                uint32_t set    = index_of(cur_addr_);
                unsigned victim = choose_victim(set);
                Line& L  = line_at(set, victim);
                L.valid  = true;
                L.tag    = tag_of(cur_addr_);
                repl_->touch(set, victim);
                phase_         = Phase::HitWait;
                hit_remaining_ = static_cast<int>(hit_latency_) - 1;
            }
            backing_.tick();
            break;
    }
}

}  // namespace cpusim
