#pragma once
#include <cstdint>
#include "cpusim/memory/mem_interface.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// IF stage — fetches one instruction per cycle.
//
// evaluate(): reads PC, loads word from instruction memory,
//             writes IfId into the output latch shadow.
//             Also computes next_pc (PC+4 or redirect target).
//
// latch():    stall  → skip; PC and latch hold their values.
//             flush  → bubble inserted; PC takes redirect_pc_.
//             normal → latch commits, PC advances to next_pc_.
//
// Control signals are set by the hazard unit (or EX stage for
// branch redirects) before latch() is called each cycle.

class FetchStage : public Stage {
public:
    FetchStage(IMemory& imem,
               Latch<pipeline::IfId>& out,
               uint32_t reset_pc = 0x80000000);

    void evaluate() override;
    void latch()    override;

    // Called by hazard unit before latch phase.
    void set_stall(bool s)                    { stall_  = s; }

    // Called by EX stage when a branch/jump is resolved.
    // Causes IF/ID flush and PC redirect next latch().
    void set_redirect(bool r, uint32_t target) {
        redirect_        = r;
        redirect_target_ = target;
    }

    uint32_t pc() const { return pc_; }

private:
    IMemory&               imem_;
    Latch<pipeline::IfId>& out_;

    uint32_t pc_;
    uint32_t next_pc_   = 0;

    bool     stall_           = false;
    bool     redirect_        = false;
    uint32_t redirect_target_ = 0;
};

}  // namespace cpusim
