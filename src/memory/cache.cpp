#include "cpusim/memory/cache.h"
#include "cpusim/memory/tree_plru.h"
#include "cpusim/memory/sequential_prefetch.h"
#include <cassert>

namespace cpusim {

static unsigned log2_exact(unsigned v) {
    assert(v && (v & (v - 1)) == 0 && "must be a power of two");
    unsigned n = 0;
    while (v > 1) { v >>= 1; ++n; }
    return n;
}

Cache::Cache(IMemory& backing, unsigned line_bytes,
             unsigned sets, unsigned hit_latency, unsigned ways,
             bool prefetch_enabled, unsigned prefetch_degree,
             SimStats* stats)
    : backing_(backing)
    , offset_bits_(log2_exact(line_bytes))
    , index_bits_(log2_exact(sets))
    , index_mask_(sets - 1)
    , hit_latency_(hit_latency ? hit_latency : 1)
    , ways_(ways)
    , lines_(static_cast<size_t>(sets) * ways)
    , repl_(std::make_unique<TreePlru>(sets, ways))
    , pf_policy_(prefetch_enabled
                 ? std::make_unique<SequentialPrefetch>(line_bytes)
                 : nullptr)
    , pf_degree_(prefetch_degree ? prefetch_degree : 1)
    , stats_(stats) {}

uint32_t Cache::index_of(uint32_t addr) const {
    return (addr >> offset_bits_) & index_mask_;
}

uint32_t Cache::tag_of(uint32_t addr) const {
    return addr >> (offset_bits_ + index_bits_);
}

uint32_t Cache::line_addr_of(uint32_t addr) const {
    return addr & ~((1u << offset_bits_) - 1u);
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

void Cache::install_line(uint32_t set, unsigned way, uint32_t addr,
                         bool is_write, bool from_prefetch) {
    Line& L = line_at(set, way);
    if (L.valid && L.prefetched && stats_) ++stats_->icache_prefetch_useless;
    L.valid      = true;
    L.dirty      = is_write;
    L.tag        = tag_of(addr);
    L.prefetched = from_prefetch;
    repl_->touch(set, way);
}

void Cache::maybe_start_prefetch(uint32_t addr) {
    if (!pf_policy_ || pf_active_) return;
    uint32_t target = pf_policy_->next(line_addr_of(addr));
    if (find_way(target) >= 0) return;   // already resident

    uint32_t set = index_of(target);
    unsigned way = choose_victim(set);
    const Line& v = line_at(set, way);
    if (v.valid && v.dirty) return;   // needs a writeback engine lacks

    pf_addr_   = target;
    pf_way_    = way;
    pf_active_ = true;
    pf_chain_  = 1;
}

void Cache::service_prefetch() {
    if (!pf_active_) return;
    if (backing_.ready(pf_addr_)) {
        install_line(index_of(pf_addr_), pf_way_, pf_addr_,
                    /*is_write=*/false, /*from_prefetch=*/true);
        pf_active_ = false;

        // Chain up to the configured degree while the port stays idle.
        if (pf_chain_ < pf_degree_) {
            uint32_t target = pf_policy_->next(pf_addr_);
            if (find_way(target) < 0) {
                uint32_t set = index_of(target);
                unsigned way = choose_victim(set);
                const Line& v = line_at(set, way);
                if (!(v.valid && v.dirty)) {
                    pf_addr_   = target;
                    pf_way_    = way;
                    pf_active_ = true;
                    pf_chain_ += 1;
                }
            }
        }
    }
    backing_.tick();
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

        // Keep the background engine fed for the line past this access;
        // a no-op if one is already queued or prefetching is disabled.
        maybe_start_prefetch(addr);

        if (last_hit_) {
            unsigned way = static_cast<unsigned>(w);
            Line& L = line_at(set, way);
            // Store hit dirties the line; no backing traffic.
            if (is_write) L.dirty = true;
            if (L.prefetched) {   // first real use of a prefetched line
                L.prefetched = false;
                if (stats_) ++stats_->icache_prefetch_useful;
            }
            repl_->touch(set, way);
            phase_         = Phase::HitWait;
            hit_remaining_ = static_cast<int>(hit_latency_) - 1;
        } else if (pf_active_ && line_addr_of(addr) == pf_addr_) {
            // Demand catches an in-flight (or queued) prefetch of the
            // same line — merge with it instead of a fresh miss.
            victim_way_ = pf_way_;
            phase_      = Phase::PrefetchWait;
        } else {
            // Pick the victim now so we know if a dirty flush is due.
            victim_way_ = choose_victim(set);
            // A queued prefetch reserved this exact way in the same
            // set for a DIFFERENT line — demand has priority; drop it
            // rather than let its later install silently clobber this
            // fill.
            // (v1 approximation: backing_ may keep whatever partial
            // progress it made on the dropped request, so this miss can
            // occasionally finish a little faster than a cold one would
            // — timing-only, never a data hazard under Option A.)
            if (pf_active_ && index_of(pf_addr_) == set && pf_way_ == victim_way_)
                pf_active_ = false;
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
            service_prefetch();   // backing port free — advance engine
            break;

        case Phase::HitWait:
            if (hit_remaining_ > 0) --hit_remaining_;
            else                    phase_ = Phase::Idle;  // served; release
            service_prefetch();   // backing port free during hit wait
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
                install_line(index_of(cur_addr_), victim_way_, cur_addr_,
                            cur_is_write_, /*from_prefetch=*/false);
                phase_         = Phase::HitWait;
                hit_remaining_ = static_cast<int>(hit_latency_) - 1;
            }
            backing_.tick();
            break;

        case Phase::PrefetchWait:
            // Take over the in-flight prefetch's backing request rather
            // than restarting — this is what makes it a PARTIAL hit:
            // only the remaining fetch time is paid, not a fresh miss.
            if (backing_.ready(cur_addr_)) {
                install_line(index_of(cur_addr_), victim_way_, cur_addr_,
                            cur_is_write_, /*from_prefetch=*/false);
                if (stats_) ++stats_->icache_prefetch_useful;
                pf_active_     = false;
                phase_         = Phase::HitWait;
                hit_remaining_ = static_cast<int>(hit_latency_) - 1;
            }
            backing_.tick();
            break;
    }
}

}  // namespace cpusim
