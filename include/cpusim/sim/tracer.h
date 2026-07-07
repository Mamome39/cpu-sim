#pragma once
#include <cstdint>
#include <ostream>
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// Emits a trace line each cycle to `out`. Two formats:
//
//   Commit — one line per retired instruction, matching Spike
//            --log-commits (RV32I, M-mode). Bubble cycles emit
//            nothing. This is the format the Spike diff compares.
//     core   0: 3 0x<pc> (0x<insn>) [x<rd>  0x<val> | mem ...]
//
//   Cycle  — one line per cycle, prefixed with the cycle number.
//            A cycle that retires nothing (stall/bubble) prints a
//            blank body, so stalls are visible as gaps.
//     <cycle> | 0x<pc> (0x<insn>) [x<rd>  0x<val> | mem ...]
//
// Attach to Core with Core::set_tracer() before running.
// The Tracer does not own the stream — caller keeps it alive.
class Tracer {
public:
    enum class Format { Commit, Cycle };

    explicit Tracer(std::ostream& out, Format fmt = Format::Commit)
        : out_(out), fmt_(fmt) {}

    // Call once per cycle with the WB-input latch and the cycle
    // index. Commit format ignores `cycle` and skips bubbles;
    // Cycle format prints every cycle.
    void record(const pipeline::MemWb& wb, uint64_t cycle);

private:
    std::ostream& out_;
    Format        fmt_;
};

}  // namespace cpusim
