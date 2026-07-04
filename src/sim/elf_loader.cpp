#include "cpusim/sim/elf_loader.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>

namespace cpusim {

// Minimal ELF32 structs — avoids <elf.h> which is Linux-only.
namespace {

struct Elf32Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

constexpr uint32_t PT_LOAD  = 1u;
constexpr uint16_t EM_RISCV = 0xF3u;

}  // namespace

uint32_t load_elf(const std::string& path, IMemory& imem, IMemory& dmem) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("elf_loader: cannot open '" + path + "'");

    Elf32Ehdr ehdr{};
    f.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
    if (!f)
        throw std::runtime_error("elf_loader: failed to read ELF header");

    // Magic + class (32-bit) + data (little-endian)
    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L'  || ehdr.e_ident[3] != 'F')
        throw std::runtime_error("elf_loader: not an ELF file");
    if (ehdr.e_ident[4] != 1)
        throw std::runtime_error("elf_loader: not ELF32");
    if (ehdr.e_ident[5] != 1)
        throw std::runtime_error("elf_loader: not little-endian ELF");
    if (ehdr.e_machine != EM_RISCV)
        throw std::runtime_error("elf_loader: not a RISC-V ELF");

    for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        Elf32Phdr phdr{};
        f.seekg(static_cast<std::streamoff>(ehdr.e_phoff) +
                i * static_cast<std::streamoff>(ehdr.e_phentsize));
        f.read(reinterpret_cast<char*>(&phdr), sizeof(phdr));
        if (!f || phdr.p_type != PT_LOAD) continue;

        std::vector<uint8_t> data(phdr.p_filesz);
        f.seekg(phdr.p_offset);
        f.read(reinterpret_cast<char*>(data.data()),
               static_cast<std::streamsize>(phdr.p_filesz));
        if (!f)
            throw std::runtime_error(
                "elf_loader: short read on segment " + std::to_string(i));

        uint32_t base = phdr.p_vaddr;
        for (uint32_t b = 0; b < phdr.p_filesz; ++b) {
            imem.store_byte(base + b, data[b]);
            dmem.store_byte(base + b, data[b]);
        }
        // Zero BSS in dmem (the portion beyond filesz up to memsz).
        for (uint32_t b = phdr.p_filesz; b < phdr.p_memsz; ++b)
            dmem.store_byte(base + b, 0);
    }

    return ehdr.e_entry;
}

}  // namespace cpusim
