#include "cpusim/uarch/pipeline/fetch.h"

namespace cpusim {

FetchStage::FetchStage(IMemory& imem,
                       Latch<pipeline::IfId>& out,
                       uint32_t reset_pc)
    : imem_(imem), out_(out), pc_(reset_pc) {}

void FetchStage::evaluate() {
    pipeline::IfId result;
    result.pc    = pc_;
    result.raw   = imem_.load_word(pc_);
    result.valid = true;
    out_.write(result);

    // Next PC: redirect takes priority over sequential advance.
    next_pc_ = redirect_ ? redirect_target_ : pc_ + 4;
}

void FetchStage::latch() {
    if (redirect_) {
        // Wrong-path instructions behind us become bubbles.
        out_.flush();
        pc_ = redirect_target_;
    } else if (!stall_) {
        out_.latch();
        pc_ = next_pc_;
    }
    // Reset transient signals — must be re-asserted every cycle.
    stall_    = false;
    redirect_ = false;
}

}  // namespace cpusim
