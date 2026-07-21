#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/memory/cache.h"
#include "cpusim/uarch/branch_predictor.h"
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/forward.h"
#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/pipeline/mem_access.h"
#include "cpusim/uarch/pipeline/writeback.h"
#include "cpusim/uarch/hazard.h"
#include "cpusim/sim/tracer.h"
#include "cpusim/sim/elf_loader.h"
#include "cpusim/sim/sim_config.h"
#include "cpusim/sim/sim_stats.h"

namespace cpusim {

// Core — wires the 5-stage pipeline and drives the clock.
//
// tick():  one clock cycle (evaluate all stages, then latch all).
//          Sets halted_ when EBREAK reaches the WB stage.
//
// run():   loops tick() until halted or max_cycles is reached.
//
// Halt condition: EBREAK retires through WB. The Core detects this
// by peeking at mem_wb_ before latch, consistent with single-cycle
// semantics (EBREAK is visible at WB input during that cycle).
//
// imem and dmem share the same base address and size but are
// independent backing stores — Harvard-style within the sim.

class Core {
public:
    // All construction parameters come from SimConfig — see
    // sim_config.h. One argument, so new knobs never change this line.
    explicit Core(const SimConfig& cfg);

    // Copy words into imem starting at base.
    void load_program(const std::vector<uint32_t>& words);

    // Load an ELF32 RV32I binary. All PT_LOAD segments go into both
    // imem and dmem. Entry point must equal base_ (enforced).
    void load_elf(const std::string& path);

    // Advance one clock cycle. Returns false if already halted.
    bool tick();

    // Run until halted or max_cycles total cycles are reached.
    void run(uint64_t max_cycles = ~0ULL);

    // Attach a tracer before running; nullptr disables tracing.
    void set_tracer(Tracer* t) { tracer_ = t; }

    // State inspection
    uint32_t read_reg(uint8_t reg)    const;
    uint32_t load_word(uint32_t addr) const;
    uint32_t pc()     const;
    uint64_t cycles() const { return cycles_; }
    bool     halted() const { return halted_; }
    uint64_t mispredicts() const { return stats_.branch_mispredicts; }
    const SimStats& stats() const { return stats_; }

    // Print all 32 registers to stdout in two-column format.
    void read_regs() const;

    // Pre-run setup (backdoor writes for test / ELF loader use)
    void write_reg(uint8_t reg, uint32_t val);
    void store_word(uint32_t addr, uint32_t val);

private:
    // During a memory stall, lock the frozen EX instruction's forwarded
    // operands into id_ex so a producer draining out of the forwarding
    // window is not lost before the stall clears.
    void capture_ex_operands();

    uint32_t base_;

    FlatMem imem_;
    FlatMem dram_;                   // backing data memory (last level)
    std::unique_ptr<Cache> dcache_;  // L1 D-cache; null unless enabled
    IMemory& dmem_;                  // MEM's port: dcache_ or dram_
    RegFile rf_;
    std::unique_ptr<BranchPredictor> bpred_;  // null unless enabled
    SimStats stats_;                          // probed by the pipeline

    Latch<pipeline::IfId>  if_id_;
    Latch<pipeline::IdEx>  id_ex_;
    Latch<pipeline::ExMem> ex_mem_;
    Latch<pipeline::MemWb> mem_wb_;

    // Declaration order matches initialisation order.
    FetchStage     fetch_;
    DecodeStage    decode_;
    ForwardUnit    fwd_;
    ExecuteStage   ex_;
    MemAccessStage mem_;
    WritebackStage wb_;
    HazardUnit     hazard_;

    Tracer*  tracer_  = nullptr;
    uint64_t cycles_ = 0;
    bool     halted_ = false;
};

}  // namespace cpusim
