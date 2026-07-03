#pragma once
#include <ostream>
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// Emits one commit-log line per retired instruction to `out`.
// Format matches Spike --log-commits (RV32I, M-mode):
//   core   0: 3 0x<pc> (0x<insn>) [<rd>  0x<val> | mem 0x<addr> 0x<val>]
//
// Attach to Core with Core::set_tracer() before running.
// The Tracer does not own the stream — caller keeps it alive.
class Tracer {
public:
    explicit Tracer(std::ostream& out) : out_(out) {}

    // Call once per cycle with the current WB-input latch.
    // No-ops if wb.valid is false.
    void record(const pipeline::MemWb& wb);

private:
    std::ostream& out_;
};

}  // namespace cpusim
