#pragma once
#include "cpusim/memory/mem_interface.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// MEM stage — data memory access.
//
// evaluate(): for a load/store, asks the memory unit whether the
//             access is served yet (dmem_.ready). While not served it
//             asserts stalling() and emits a bubble downstream. On the
//             served cycle it performs the access and produces MemWb.
//             Non-memory ops pass alu_out through in one cycle.
//
//             The MEM stage is a thin client: it only knows served vs
//             not-served. The latency itself lives in the memory unit,
//             which lets a cache (fast hit / slow miss) drop in with no
//             change here.
//
// latch():    advances the memory timing (dmem_.tick) and commits the
//             MemWb shadow.
//
// The pipeline freeze during a stall is orchestrated by Core: it reads
// stalling() and holds the upstream latches (IF/ID/EX) for that cycle.

class MemAccessStage : public Stage {
public:
    MemAccessStage(IMemory&                      dmem,
                   const Latch<pipeline::ExMem>& in,
                   Latch<pipeline::MemWb>&       out);

    void evaluate() override;
    void latch()    override;

    // True on cycles where MEM is waiting on a not-yet-served access.
    bool stalling() const { return stalling_; }

private:
    IMemory&                      dmem_;
    const Latch<pipeline::ExMem>& in_;
    Latch<pipeline::MemWb>&       out_;

    bool stalling_ = false;
};

}  // namespace cpusim
