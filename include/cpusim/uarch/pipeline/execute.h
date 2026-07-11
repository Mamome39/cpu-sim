#pragma once
#include <cstdint>
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/forward.h"
#include "cpusim/uarch/branch_predictor.h"

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
    // bp may be null — then EX never trains and every taken control
    // transfer is a redirect (identical to no-predictor behavior).
    ExecuteStage(const Latch<pipeline::IdEx>& in,
                 Latch<pipeline::ExMem>&      out,
                 ForwardUnit&                 fwd,
                 FetchStage&                  fetch,
                 DecodeStage&                 decode,
                 BranchPredictor*             bp = nullptr);

    void evaluate() override;
    void latch()    override;

    // Control-flow redirects (mispredictions) taken so far.
    uint64_t mispredicts() const { return mispredicts_; }

private:
    const Latch<pipeline::IdEx>& in_;
    Latch<pipeline::ExMem>&      out_;
    ForwardUnit&                 fwd_;
    FetchStage&                  fetch_;
    DecodeStage&                 decode_;
    BranchPredictor*             bp_;

    // Set in evaluate(), applied in latch().
    bool     redirect_        = false;
    uint32_t redirect_target_ = 0;
    bool     train_           = false;  // train predictor this instr
    uint32_t train_pc_        = 0;
    bool     train_cond_      = false;
    bool     train_taken_     = false;
    uint32_t train_target_    = 0;

    uint64_t mispredicts_     = 0;
};

}  // namespace cpusim
