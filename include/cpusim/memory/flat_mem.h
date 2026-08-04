#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "cpusim/memory/mem_interface.h"

// Flat byte-addressable memory backed by a vector.
// Covers addresses [base, base + size).
// Used as the backing store for cache models and directly
// in tests where cache behaviour is not under test.

namespace cpusim {

class FlatMem : public IMemory {
public:
    // latency = cycles to serve one access (>= 1). 1 = single-cycle.
    FlatMem(uint32_t base, size_t size_bytes, unsigned latency = 1);

    uint32_t load_word(uint32_t addr) const override;
    uint16_t load_half(uint32_t addr) const override;
    uint8_t  load_byte(uint32_t addr) const override;

    void store_word(uint32_t addr, uint32_t val) override;
    void store_half(uint32_t addr, uint16_t val) override;
    void store_byte(uint32_t addr, uint8_t  val) override;

    // Timing model: a request takes `latency` cycles to serve.
    // is_write is ignored — flat memory has no dirty state.
    bool ready(uint32_t addr, bool is_write) override;
    void tick() override;

    // Write raw bytes into memory — used to load programs.
    void write_bytes(uint32_t addr,
                     const uint8_t* data, size_t len);

    uint32_t base() const { return base_; }
    size_t   size() const { return mem_.size(); }

private:
    uint32_t             base_;
    std::vector<uint8_t> mem_;

    // Timing state — one outstanding request (blocking, in-order).
    unsigned latency_;
    bool     serving_   = false;
    int      remaining_ = 0;

    size_t offset(uint32_t addr) const;
};

}  // namespace cpusim
