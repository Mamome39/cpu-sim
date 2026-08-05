#include "cpusim/uarch/pipeline/fetch.h"

namespace cpusim {

FetchStage::FetchStage(IMemory& imem,
                       Latch<pipeline::IfId>& out,
                       uint32_t reset_pc,
                       BranchPredictor* bp)
    : imem_(imem), out_(out), bp_(bp), pc_(reset_pc) {}

void FetchStage::evaluate() {
    // Ask the instruction memory whether this fetch is served yet.
    // While it is not, stall the front end and emit a bubble; the
    // load runs only on the served cycle (mirrors MemAccessStage).
    fetch_stall_ = !imem_.ready(pc_);
    if (fetch_stall_) {
        out_.write(pipeline::IfId{});   // bubble while the I-miss resolves
        next_pc_ = pc_;                 // PC holds; latch() enforces this
        return;
    }

    pipeline::IfId result;
    result.pc    = pc_;
    result.raw   = imem_.load_word(pc_);
    result.valid = true;

    // Predict this instruction's next PC. A predicted-taken branch
    // steers the next fetch to the target with no bubble; the guess
    // rides down the pipe for EX to check.
    uint32_t predicted_pc = pc_ + 4;
    if (bp_) {
        BranchPredictor::Prediction p = bp_->predict(pc_);
        if (p.taken) {
            result.predicted_taken = true;
            predicted_pc           = p.target;
        }
    }
    out_.write(result);

    // A misprediction redirect from EX overrides the local prediction.
    next_pc_ = redirect_ ? redirect_target_ : predicted_pc;
}

void FetchStage::latch() {
    imem_.tick();   // advance the instruction memory's timing every cycle

    if (redirect_) {
        // Wrong-path instructions behind us become bubbles.
        out_.flush();
        pc_ = redirect_target_;
    } else if (!stall_) {
        out_.latch();                   // commits real instr or I-miss bubble
        if (!fetch_stall_) pc_ = next_pc_;   // hold PC while the I-miss resolves
    }
    // Reset transient signals — must be re-asserted every cycle.
    stall_    = false;
    redirect_ = false;
}

}  // namespace cpusim
