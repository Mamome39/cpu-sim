#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace cpusim {

// 32 general-purpose registers for RV32I.
// x0 is hardwired to zero: reads return 0, writes are dropped.
class RegFile {
public:
    static constexpr std::size_t kNumRegs = 32;
    using value_t = std::uint32_t;

    RegFile() : regs_{} {}

    value_t read(std::size_t idx) const {
        assert(idx < kNumRegs && "register index out of range");
        return regs_[idx];          // x0 stays 0 because we never write it
    }

    void write(std::size_t idx, value_t value) {
        assert(idx < kNumRegs && "register index out of range");
        if (idx == 0) return;       // x0 is hardwired to zero
        regs_[idx] = value;
    }

    void reset() { regs_.fill(0); }

    // Read-only view for dumps and trace formatting.
    const std::array<value_t, kNumRegs>& raw() const { return regs_; }

    bool operator==(const RegFile& other) const { return regs_ == other.regs_; }
    bool operator!=(const RegFile& other) const { return !(*this == other); }

private:
    std::array<value_t, kNumRegs> regs_;
};

// RV32I psABI register names. Useful for human-readable traces.
inline const char* abi_name(std::size_t idx) {
    static constexpr const char* kNames[32] = {
        "zero", "ra",  "sp",  "gp",  "tp",  "t0",  "t1",  "t2",
        "s0",   "s1",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
        "a6",   "a7",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
        "s8",   "s9",  "s10", "s11", "t3",  "t4",  "t5",  "t6",
    };
    assert(idx < 32);
    return kNames[idx];
}

}  // namespace cpusim