#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/memory/cache.h"
#include "cpusim/memory/unified_l2.h"
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

// Why the Core stopped. Both halt conditions are detected at the same
// point — an instruction retiring through WB — so neither can be
// triggered by a speculative instruction that a branch flush kills.
enum class HaltReason : uint8_t {
    None,     // still running
    Ebreak,   // EBREAK retired — the normal program exit
    Illegal,  // an illegal instruction retired — a bug, not an exit
};

// Core — wires the 5-stage pipeline and drives the clock.
//
// tick():  one clock cycle (evaluate all stages, then latch all).
//          Sets halted_ when EBREAK or an illegal instruction
//          reaches the WB stage.
//
// run():   loops tick() until halted or max_cycles is reached.
//
// Halt condition: EBREAK retires through WB. The Core detects this
// by peeking at mem_wb_ before latch, consistent with single-cycle
// semantics (EBREAK is visible at WB input during that cycle).
//
// An illegal instruction halts the same way. Real hardware would trap;
// with no CSRs there is no trap vector to take, and continuing is
// worse than stopping — silently retiring an undefined encoding as a
// no-op lets a program run through garbage and still report success.
// halt_reason() distinguishes the two, and halt_pc()/halt_raw() say
// which encoding was at fault.
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
    uint64_t cycles() const { return stats_.cycles; }
    uint64_t instructions() const { return stats_.instructions; }
    bool     halted() const { return halted_; }

    // Why the run stopped, and the instruction responsible. Only
    // meaningful once halted(); halt_pc()/halt_raw() are set for both
    // reasons, but matter mainly for HaltReason::Illegal.
    HaltReason halt_reason() const { return halt_reason_; }
    uint32_t   halt_pc()     const { return halt_pc_; }
    uint32_t   halt_raw()    const { return halt_raw_; }
    uint64_t mispredicts() const { return stats_.branch_mispredicts; }
    const SimStats& stats() const { return stats_; }

    // The shared L2, or nullptr when disabled — for its hit/miss
    // counts. Kept off SimStats because a level owns its own counters.
    const UnifiedL2* l2() const { return l2_.get(); }

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

    // Declared first: the memory hierarchy binds to stats_.cycles as
    // the clock a shared level arbitrates on (see unified_l2.h), so it
    // has to exist before any of it is built.
    SimStats stats_;                 // probed by the pipeline

    uint32_t base_;

    // Built inner-to-outer, so every IMemory& binds to a level that is
    // already constructed: the two data authorities, then the shared
    // L2 and its per-side ports, then the L1s, then each stage's port.
    FlatMem imem_;                   // instruction-side data authority
    FlatMem dram_;                   // data-side authority + last level
    std::unique_ptr<UnifiedL2> l2_;  // shared L2; null unless enabled
    L2Port*  il2_;                   // I-side L2 port; null with no L2
    L2Port*  dl2_;                   // D-side L2 port; null with no L2
    std::unique_ptr<Cache> icache_;  // L1 I-cache; null unless enabled
    IMemory& iport_;                 // fetch's port: icache_/L2/imem_
    std::unique_ptr<Cache> dcache_;  // L1 D-cache; null unless enabled
    IMemory& dmem_;                  // MEM's port: dcache_/L2/dram_
    RegFile rf_;
    std::unique_ptr<BranchPredictor> bpred_;  // null unless enabled

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
    bool     halted_  = false;

    HaltReason halt_reason_ = HaltReason::None;
    uint32_t   halt_pc_     = 0;
    uint32_t   halt_raw_    = 0;
};

}  // namespace cpusim
