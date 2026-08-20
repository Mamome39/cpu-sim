#include "cpusim/memory/unified_l2.h"

namespace cpusim {

// ── L2Port ────────────────────────────────────────────────────────────

L2Port::L2Port(UnifiedL2& hub, unsigned id, IMemory& data)
    : hub_(hub), id_(id), data_(data) {}

// Data path — this side's own authority, never the shared L2. Keeping
// the two sides' backing stores separate here is what preserves the
// Harvard split (and the byte-identical-trace guarantee) when the L2
// is enabled. See the class comment.
uint32_t L2Port::load_word(uint32_t addr) const { return data_.load_word(addr); }
uint16_t L2Port::load_half(uint32_t addr) const { return data_.load_half(addr); }
uint8_t  L2Port::load_byte(uint32_t addr) const { return data_.load_byte(addr); }
void L2Port::store_word(uint32_t addr, uint32_t val) { data_.store_word(addr, val); }
void L2Port::store_half(uint32_t addr, uint16_t val) { data_.store_half(addr, val); }
void L2Port::store_byte(uint32_t addr, uint8_t  val) { data_.store_byte(addr, val); }

// Timing path — the shared L2, via the hub's arbitration.
bool L2Port::ready(uint32_t addr, bool is_write) {
    return hub_.port_ready(id_, addr, is_write);
}

void L2Port::tick() { hub_.port_tick(); }

// ── UnifiedL2 ─────────────────────────────────────────────────────────

UnifiedL2::UnifiedL2(IMemory& backing, const uint64_t& cycle,
                     unsigned line_bytes, unsigned sets,
                     unsigned hit_latency, unsigned ways)
    : l2_(backing, line_bytes, sets, hit_latency, ways)
    , cycle_(cycle) {}

L2Port& UnifiedL2::attach(IMemory& data) {
    unsigned id = static_cast<unsigned>(ports_.size());
    ports_.push_back(std::make_unique<L2Port>(*this, id, data));
    return *ports_.back();
}

// Rule 3 — a port refused while another held the L2 is next in line,
// and the port that just gave it up waits its turn. The claim is only
// honoured while the waiter is still asking (it re-stamps the cycle on
// every refusal): a port that abandons a request — a prefetch dropped
// for a colliding demand — must not be able to wedge the other side by
// holding a reservation it will never come back for.
bool UnifiedL2::reserved_against(unsigned id) const {
    return waiter_ != kFree && waiter_ != id
        && id == last_owner_
        && cycle_ - waiter_cyc_ <= 1;
}

// Rule 1 — ownership. A free L2 is claimed by whoever asks first (rule
// 3 permitting) and held until the access completes; anyone else is
// simply not ready and retries next cycle, exactly as it would against
// a busy single cache.
bool UnifiedL2::port_ready(unsigned id, uint32_t addr, bool is_write) {
    if (owner_ == kFree && reserved_against(id)) {
        // Free, but promised to the other side. Leaving it idle for a
        // cycle is the cost of fairness, and the waiter claims it in
        // this same cycle whenever it asks after us.
        return false;
    }
    if (owner_ != kFree && owner_ != id) {
        waiter_     = id;        // remember, and re-stamp, who is waiting
        waiter_cyc_ = cycle_;
        return false;            // busy elsewhere
    }

    owner_      = id;
    last_owner_ = id;
    if (waiter_ == id) waiter_ = kFree;   // claimed by its waiter
    return l2_.ready(addr, is_write);
}

// Rule 2 — one clock, one advance, driven by ANY port.
//
// The dedupe is what makes "any port" safe: two L1s driving the L2 in
// the same cycle must not advance it twice (that would serve a miss at
// half its latency). The owner is released the moment the L2 falls
// idle, which is the cycle after it announced ready() — so the next
// requester can claim it without a gap it did not earn.
void UnifiedL2::port_tick() {
    if (last_tick_ == cycle_) return;   // already advanced this cycle
    last_tick_ = cycle_;

    l2_.tick();
    if (l2_.idle()) owner_ = kFree;
}

}  // namespace cpusim
