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
};

}  // namespace cpusim
