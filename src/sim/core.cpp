#include "cpusim/sim/core.h"
#include "cpusim/isa/rv32i/decoder.h"
#include <cstdio>
#include <stdexcept>
#include <string>

namespace cpusim {

Core::Core(const SimConfig& cfg)
    : base_(cfg.ram_base_addr)
    , imem_(cfg.ram_base_addr, cfg.ram_size_bytes, cfg.imem_latency_cycles)
    , icache_(cfg.icache_enabled
              ? std::make_unique<Cache>(imem_, cfg.icache_line_bytes,
                                        cfg.icache_sets,
                                        cfg.icache_hit_latency_cycles,
                                        cfg.icache_ways,
                                        cfg.icache_prefetch_enabled,
                                        cfg.icache_prefetch_degree,
                                        &stats_)
              : nullptr)
    , iport_(icache_ ? static_cast<IMemory&>(*icache_) : imem_)
    , dram_(cfg.ram_base_addr, cfg.ram_size_bytes, cfg.dmem_latency_cycles)
    , dcache_(cfg.dcache_enabled
              ? std::make_unique<Cache>(dram_, cfg.dcache_line_bytes,
                                        cfg.dcache_sets,
                                        cfg.dcache_hit_latency_cycles,
                                        cfg.dcache_ways)
              : nullptr)
    , dmem_(dcache_ ? static_cast<IMemory&>(*dcache_) : dram_)
    , bpred_(cfg.bpred_enabled
             ? std::make_unique<BranchPredictor>(cfg.bpred_bht_entries,
                                                 cfg.bpred_btb_entries)
             : nullptr)
    , fetch_(iport_, if_id_, cfg.ram_base_addr, bpred_.get())
    , decode_(rf_, if_id_, id_ex_)
    , fwd_(ex_mem_, mem_wb_)
    , ex_(id_ex_, ex_mem_, fwd_, fetch_, decode_, bpred_.get(), &stats_)
    , mem_(dmem_, ex_mem_, mem_wb_)
    , wb_(rf_, mem_wb_)
    , hazard_(if_id_, id_ex_, fetch_, decode_)
{}

void Core::load_elf(const std::string& path) {
    uint32_t entry = cpusim::load_elf(path, imem_, dmem_);
    if (entry != base_) {
        char msg[80];
        std::snprintf(msg, sizeof(msg),
            "load_elf: entry 0x%08x != base 0x%08x", entry, base_);
        throw std::runtime_error(msg);
    }
}

void Core::load_program(const std::vector<uint32_t>& words) {
    uint32_t addr = base_;
    for (uint32_t w : words) {
        imem_.store_word(addr, w);
        addr += 4;
    }
}

bool Core::tick() {
    if (halted_) return false;

    fetch_.evaluate();
    decode_.evaluate();
    fwd_.evaluate();
    ex_.evaluate();
    mem_.evaluate();
    wb_.evaluate();
    hazard_.evaluate();

    // A memory access that is not yet served stalls the pipeline. The
    // stall propagates BACKWARD only: upstream (IF/ID/EX) freezes since
    // MEM is occupied, while downstream (WB) drains normally — the
    // instruction already past MEM is done and retires regardless.
    const bool mem_stall = mem_.stalling();

    const pipeline::MemWb& wb_in = mem_wb_.read();
    if (tracer_) tracer_->record(wb_in, stats_.cycles);

    // Detect a halting instruction reaching WB before any latch
    // commits. Checking at WB rather than at decode is what makes this
    // speculation-safe: a wrong-path instruction is flushed before it
    // ever arrives here, so garbage fetched past a mispredicted branch
    // cannot stop the machine.
    if (wb_in.valid && (wb_in.op == rv32i::Op::EBREAK ||
                        wb_in.op == rv32i::Op::ILLEGAL)) {
        halted_      = true;
        halt_reason_ = wb_in.op == rv32i::Op::EBREAK ? HaltReason::Ebreak
                                                     : HaltReason::Illegal;
        halt_pc_     = wb_in.pc;
        halt_raw_    = wb_in.raw;
    }

    // WB drains on both paths below, so a valid instruction at the
    // MEM/WB input retires this cycle either way. A MEM stall pushes
    // a bubble into mem_wb_, so no instruction is counted twice.
    if (wb_in.valid) ++stats_.instructions;

    if (mem_stall) {
        // Preserve the frozen EX instruction's operands before the
        // producers behind them drain out of the forwarding window.
        capture_ex_operands();

        wb_.latch();    // downstream drains: instruction ahead retires
        fwd_.latch();
        mem_.latch();   // MEM/WB gets a bubble; memory timer advances
        // ex_/hazard_/decode_/fetch_ NOT latched → IF/ID/EX + PC hold.
        ++stats_.cycles;
        return !halted_;
    }

    wb_.latch();
    fwd_.latch();
    ex_.latch();
    mem_.latch();
    hazard_.latch();
    decode_.latch();
    fetch_.latch();

    ++stats_.cycles;
    return !halted_;
}

void Core::capture_ex_operands() {
    // Resolve the EX instruction's operands through forwarding while the
    // producers are still in the window (they drain this cycle), and
    // write the values back into id_ex. On later stall cycles resolve()
    // finds no match and falls back to these captured values, so EX
    // reads correct operands when it finally executes. The load-use
    // interlock guarantees this instruction never depends on the
    // stalling load itself, so ex_mem (the load) is never a false match.
    pipeline::IdEx held = id_ex_.read();
    if (!held.valid) return;   // bubble in EX — nothing to preserve
    held.rs1_val = fwd_.resolve(held.rs1, held.rs1_val);
    held.rs2_val = fwd_.resolve(held.rs2, held.rs2_val);
    id_ex_.write(held);
    id_ex_.latch();
}

void Core::run(uint64_t max_cycles) {
    while (!halted_ && stats_.cycles < max_cycles)
        tick();
}

uint32_t Core::read_reg(uint8_t reg) const {
    return rf_.read(reg);
}

uint32_t Core::load_word(uint32_t addr) const {
    return dmem_.load_word(addr);
}

uint32_t Core::pc() const {
    return fetch_.pc();
}

void Core::write_reg(uint8_t reg, uint32_t val) {
    rf_.write(reg, val);
}

void Core::store_word(uint32_t addr, uint32_t val) {
    dmem_.store_word(addr, val);
}

void Core::read_regs() const {
    for (int i = 0; i < 16; ++i) {
        int j = i + 16;
        std::printf(" x%-2d %4s = 0x%08X    x%-2d %4s = 0x%08X\n",
                    i, abi_name(i),    rf_.read(i),
                    j, abi_name(j),    rf_.read(j));
    }
}

}  // namespace cpusim
