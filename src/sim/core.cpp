#include "cpusim/sim/core.h"
#include "cpusim/isa/rv32i/decoder.h"
#include <cstdio>

namespace cpusim {

Core::Core(uint32_t base, size_t mem_size)
    : base_(base)
    , imem_(base, mem_size)
    , dmem_(base, mem_size)
    , fetch_(imem_, if_id_, base)
    , decode_(rf_, if_id_, id_ex_)
    , fwd_(ex_mem_, mem_wb_)
    , ex_(id_ex_, ex_mem_, fwd_, fetch_, decode_)
    , mem_(dmem_, ex_mem_, mem_wb_)
    , wb_(rf_, mem_wb_)
    , hazard_(if_id_, id_ex_, fetch_, decode_)
{}

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

    // Detect EBREAK reaching WB before any latch commits.
    const pipeline::MemWb& wb_in = mem_wb_.read();
    if (wb_in.valid && wb_in.op == rv32i::Op::EBREAK)
        halted_ = true;

    wb_.latch();
    fwd_.latch();
    ex_.latch();
    mem_.latch();
    hazard_.latch();
    decode_.latch();
    fetch_.latch();

    ++cycles_;
    return !halted_;
}

void Core::run(uint64_t max_cycles) {
    while (!halted_ && cycles_ < max_cycles)
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
