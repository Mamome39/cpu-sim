#include "cpusim/memory/cache.h"
#include <cassert>

namespace cpusim {

static unsigned log2_exact(unsigned v) {
    assert(v && (v & (v - 1)) == 0 && "must be a power of two");
    unsigned n = 0;
    while (v > 1) { v >>= 1; ++n; }
    return n;
}

Cache::Cache(IMemory& backing, unsigned line_bytes,
             unsigned sets, unsigned hit_latency)
    : backing_(backing)
    , offset_bits_(log2_exact(line_bytes))
    , index_bits_(log2_exact(sets))
    , index_mask_(sets - 1)
    , hit_latency_(hit_latency ? hit_latency : 1)
    , valid_(sets, false)
    , tag_(sets, 0) {}

uint32_t Cache::index_of(uint32_t addr) const {
    return (addr >> offset_bits_) & index_mask_;
}

uint32_t Cache::tag_of(uint32_t addr) const {
    return addr >> (offset_bits_ + index_bits_);
}

bool Cache::is_hit(uint32_t addr) const {
    uint32_t i = index_of(addr);
    return valid_[i] && tag_[i] == tag_of(addr);
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
        last_hit_ = is_hit(addr);
        if (last_hit_) {
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
            // install it and finish the access at hit latency.
            if (backing_.ready(cur_addr_)) {
                uint32_t i = index_of(cur_addr_);
                valid_[i] = true;
                tag_[i]   = tag_of(cur_addr_);
                phase_         = Phase::HitWait;
                hit_remaining_ = static_cast<int>(hit_latency_) - 1;
            }
            backing_.tick();
            break;
    }
}

}  // namespace cpusim
