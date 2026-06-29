#pragma once
#include "cpusim/memory/mem_interface.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// MEM stage — data memory access.
//
// evaluate(): for load ops, issues the load to dmem and places the
//             result in wb_val. For store ops, issues the store and
//             passes alu_out (address) as wb_val (unused by WB).
//             Non-memory ops pass alu_out through unchanged.
//             Propagates a bubble when upstream is invalid.
//
// latch():    commits MemWb shadow. No stall/flush inputs — hazards
//             from loads are handled upstream by the hazard unit.

class MemAccessStage : public Stage {
public:
    MemAccessStage(IMemory&                      dmem,
                   const Latch<pipeline::ExMem>& in,
                   Latch<pipeline::MemWb>&       out);

    void evaluate() override;
    void latch()    override;

private:
    IMemory&                      dmem_;
    const Latch<pipeline::ExMem>& in_;
    Latch<pipeline::MemWb>&       out_;
};

}  // namespace cpusim
