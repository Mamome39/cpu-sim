#include "cpusim/memory/flat_mem.h"
#include <cassert>
#include <cstring>

namespace cpusim {

FlatMem::FlatMem(uint32_t base, size_t size_bytes)
    : base_(base), mem_(size_bytes, 0) {}

size_t FlatMem::offset(uint32_t addr) const {
    assert(addr >= base_ && addr - base_ < mem_.size()
           && "address out of range");
    return addr - base_;
}

uint8_t FlatMem::load_byte(uint32_t addr) const {
    return mem_[offset(addr)];
}

uint16_t FlatMem::load_half(uint32_t addr) const {
    size_t o = offset(addr);
    return static_cast<uint16_t>(mem_[o])
         | static_cast<uint16_t>(mem_[o + 1]) << 8;
}

uint32_t FlatMem::load_word(uint32_t addr) const {
    size_t o = offset(addr);
    return static_cast<uint32_t>(mem_[o])
         | static_cast<uint32_t>(mem_[o + 1]) << 8
         | static_cast<uint32_t>(mem_[o + 2]) << 16
         | static_cast<uint32_t>(mem_[o + 3]) << 24;
}

void FlatMem::store_byte(uint32_t addr, uint8_t val) {
    mem_[offset(addr)] = val;
}

void FlatMem::store_half(uint32_t addr, uint16_t val) {
    size_t o = offset(addr);
    mem_[o]     = val & 0xff;
    mem_[o + 1] = (val >> 8) & 0xff;
}

void FlatMem::store_word(uint32_t addr, uint32_t val) {
    size_t o = offset(addr);
    mem_[o]     =  val        & 0xff;
    mem_[o + 1] = (val >>  8) & 0xff;
    mem_[o + 2] = (val >> 16) & 0xff;
    mem_[o + 3] = (val >> 24) & 0xff;
}

void FlatMem::write_bytes(uint32_t addr,
                          const uint8_t* data, size_t len) {
    size_t o = offset(addr);
    assert(o + len <= mem_.size() && "write_bytes out of range");
    std::memcpy(mem_.data() + o, data, len);
}

}  // namespace cpusim
