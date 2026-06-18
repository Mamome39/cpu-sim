#pragma once
#include <cstdint>
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"

namespace cpusim {

// EX stage — ALU execution and branch resolution.
//
// evaluate(): reads IdEx, runs ALU, evaluates branch condition,
//             computes branch target (pc + imm), writes ExMem shadow.
//             Propagates a bubble when upstream is invalid.
//
// latch():    commits ExMem shadow. If branch taken, signals
//             FetchStage (redirect) and DecodeStage (flush) before
//             committing — those stages honour the signals on their
//             own latch() calls in the same clock phase.
//
// No stall input: EX does not stall in this baseline design.
// Hazard stalls are inserted upstream (ID/IF).

class ExecuteStage : public Stage {
public:
    ExecuteStage(const Latch<pipeline::IdEx>&  in,
                 Latch<pipeline::ExMem>&       out,
                 FetchStage&                   fetch,
                 DecodeStage&                  decode);

    void evaluate() override;
    void latch()    override;

private:
    const Latch<pipeline::IdEx>& in_;
    Latch<pipeline::ExMem>&      out_;
    FetchStage&                  fetch_;
    DecodeStage&                 decode_;

    // Computed in evaluate(), consumed in latch().
    bool     branch_taken_  = false;
    uint32_t branch_target_ = 0;
};

}  // namespace cpusim
