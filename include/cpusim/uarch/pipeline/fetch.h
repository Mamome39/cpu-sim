#pragma once
#include <cstdint>
#include "cpusim/memory/mem_interface.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/branch_predictor.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// IF stage — fetches one instruction per cycle from a ready()/tick()
// instruction memory (a real I-cache, or a latency-1 FlatMem).
//
// evaluate(): asks imem_.ready(pc_) first. Not ready (I-miss in
//             progress) → write a bubble, hold PC, set stalling().
//             Ready → load the word, write IfId, compute next_pc
//             (PC+4 or redirect target).
//
// latch():    imem_.tick() advances every cycle (mirrors MEM). Then:
//             hazard stall → skip; PC and if_id hold their values.
//             flush         → bubble inserted; PC takes redirect_pc_.
//             normal        → latch commits (real instr or I-miss
//                              bubble); PC advances unless stalling().
//
// Control signals are set by the hazard unit (or EX stage for
// branch redirects) before latch() is called each cycle. A front-end
// (I-miss) stall is orchestrated by Core reading stalling(): downstream
// stages keep draining while IF/ID takes a bubble and PC holds — see
// Core::tick().

class FetchStage : public Stage {
public:
    // bp may be null — then IF always predicts not-taken (base behavior).
    FetchStage(IMemory& imem,
               Latch<pipeline::IfId>& out,
               uint32_t reset_pc = 0x80000000,
               BranchPredictor* bp = nullptr);

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

    // True on cycles where IF is waiting on a not-yet-served I-cache
    // access (front-end stall — see Core::tick()).
    bool stalling() const { return fetch_stall_; }

private:
    IMemory&               imem_;
    Latch<pipeline::IfId>& out_;
    BranchPredictor*       bp_;

    uint32_t pc_;
    uint32_t next_pc_   = 0;

    bool     stall_           = false;
    bool     redirect_        = false;
    uint32_t redirect_target_ = 0;
    bool     fetch_stall_     = false;   // I-miss in progress
};

}  // namespace cpusim
