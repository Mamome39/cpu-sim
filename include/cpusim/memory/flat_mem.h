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
    FlatMem(uint32_t base, size_t size_bytes);

    uint32_t load_word(uint32_t addr) const override;
    uint16_t load_half(uint32_t addr) const override;
    uint8_t  load_byte(uint32_t addr) const override;

    void store_word(uint32_t addr, uint32_t val) override;
    void store_half(uint32_t addr, uint16_t val) override;
    void store_byte(uint32_t addr, uint8_t  val) override;

    // Write raw bytes into memory — used to load programs.
    void write_bytes(uint32_t addr,
                     const uint8_t* data, size_t len);

    uint32_t base() const { return base_; }
    size_t   size() const { return mem_.size(); }

private:
    uint32_t           base_;
    std::vector<uint8_t> mem_;

    size_t offset(uint32_t addr) const;
};

}  // namespace cpusim
