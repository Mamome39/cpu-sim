#pragma once
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"

namespace cpusim {

// HazardUnit — detects load-use hazards and inserts stall bubbles.
//
// A load-use hazard occurs when a load (LB/LH/LW/LBU/LHU) is in EX
// and the very next instruction (currently in ID) reads the same rd.
// Forwarding cannot help here: the loaded value isn't available until
// the end of MEM, one cycle too late for EX in the next cycle.
//
// evaluate(): reads id_ex (load in EX) and if_id (consumer in ID).
//             Sets stall_ if a load-use hazard is detected.
//             rs1/rs2 are extracted conservatively from the raw word.
//
// latch():    when stall_ is set, signals FetchStage to stall
//             (PC + if_id frozen) and DecodeStage to flush
//             (bubble inserted into id_ex). Both signals are consumed
//             by those stages in the same latch phase.

class HazardUnit : public Stage {
public:
    HazardUnit(const Latch<pipeline::IfId>&  if_id,
               const Latch<pipeline::IdEx>&  id_ex,
               FetchStage&                   fetch,
               DecodeStage&                  decode);

    void evaluate() override;
    void latch()    override;

private:
    const Latch<pipeline::IfId>& if_id_;
    const Latch<pipeline::IdEx>& id_ex_;
    FetchStage&                  fetch_;
    DecodeStage&                 decode_;

    bool stall_ = false;

    static bool is_load(rv32i::Op op);
};

}  // namespace cpusim
