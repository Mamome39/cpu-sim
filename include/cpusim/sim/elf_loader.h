#pragma once
#include <string>
#include "cpusim/memory/mem_interface.h"

namespace cpusim {

// Load an ELF32 RISC-V binary into instruction and data memories.
//
// All PT_LOAD segments are written byte-by-byte into both imem and
// dmem. This matches the Harvard-style sim: imem serves instruction
// fetches, dmem serves load/store, but both cover the same physical
// address range so a single flat binary works in either memory.
// BSS (memsz > filesz) is zeroed in dmem.
//
// Returns e_entry (the ELF entry point address).
// Throws std::runtime_error on I/O or parse errors.
uint32_t load_elf(const std::string& path, IMemory& imem, IMemory& dmem);

}  // namespace cpusim
