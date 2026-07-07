#pragma once
#include <cstdint>

// Abstract memory interface.
// All levels of the hierarchy (flat, L1, L2) implement this.
// Loads return the value; stores write it.
// Byte order is little-endian throughout.
// Word = 4 bytes, half = 2 bytes, byte = 1 byte.

namespace cpusim {

class IMemory {
public:
    virtual ~IMemory() = default;

    virtual uint32_t load_word(uint32_t addr) const = 0;
    virtual uint16_t load_half(uint32_t addr) const = 0;
    virtual uint8_t  load_byte(uint32_t addr) const = 0;

    virtual void store_word(uint32_t addr, uint32_t val) = 0;
    virtual void store_half(uint32_t addr, uint16_t val) = 0;
    virtual void store_byte(uint32_t addr, uint8_t  val) = 0;

    // ── Timing model ─────────────────────────────────────────────
    // Separate from the (always-correct, immediate) data path above.
    // A caller that models timing asks ready() each cycle and stalls
    // until it returns true; tick() advances the model one cycle.
    // The data itself is still read/written via the load/store calls
    // on the served cycle, so timing never changes values.
    //
    // ready(addr): true the cycle an access to addr is served. Lazily
    //   starts the request on the first call. Default: single-cycle
    //   memory — always ready.
    // tick(): advance the timing model one cycle. Default: no-op.
    virtual bool ready(uint32_t /*addr*/) { return true; }
    virtual void tick() {}
};

}  // namespace cpusim
