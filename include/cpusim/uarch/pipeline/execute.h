#pragma once
#include <cstdint>
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/forward.h"

namespace cpusim {

// EX stage — ALU execution, branch resolution.
//
// evaluate(): reads IdEx, calls ForwardUnit::resolve() for rs1/rs2
//             operands, runs ALU, evaluates branch condition,
//             writes ExMem shadow. Bubbles propagate unchanged.
//
// latch():    commits ExMem shadow. If branch taken, signals
//             FetchStage (redirect) and DecodeStage (flush).

class ExecuteStage : public Stage {
public:
    ExecuteStage(const Latch<pipeline::IdEx>& in,
                 Latch<pipeline::ExMem>&      out,
                 ForwardUnit&                 fwd,
                 FetchStage&                  fetch,
                 DecodeStage&                 decode);

    void evaluate() override;
    void latch()    override;

private:
    const Latch<pipeline::IdEx>& in_;
    Latch<pipeline::ExMem>&      out_;
    ForwardUnit&                 fwd_;
    FetchStage&                  fetch_;
    DecodeStage&                 decode_;

    bool     branch_taken_  = false;
    uint32_t branch_target_ = 0;
};

}  // namespace cpusim
