// SPDX-License-Identifier: GPL-3.0-or-later
// Hand-assembled AArch64 test program: can the TV (aarch64 kernel, 32-bit userland) run a 64-bit
// process for us? Written to tmpfs at run time, so the ipk itself only contains ARM32 ELFs.
#include <cstdint>
#include <cstring>
#include <vector>

#include "child_probe.hpp"

namespace csprobe {
namespace {

void put(std::vector<unsigned char> &b, uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) b.push_back(static_cast<unsigned char>(v >> (8 * i)));
}

}  // namespace

std::vector<unsigned char> aarch64_exit42_elf() {
    constexpr uint64_t kBase = 0x400000;
    constexpr uint64_t kCode = 64 + 56;  // ELF header + one program header
    static const char kMsg[] = "a64 ok\n";
    constexpr uint32_t kMsgLen = sizeof(kMsg) - 1;
    const uint32_t code[] = {
            0xD2800020,                   // movz x0, #1          (stdout)
            0x100000E1,                   // adr  x1, msg         (+28 from this instruction)
            0xD2800002 | (kMsgLen << 5),  // movz x2, #len
            0xD2800808,                   // movz x8, #64         (write)
            0xD4000001,                   // svc  #0
            0xD2800540,                   // movz x0, #42
            0xD2800BA8,                   // movz x8, #93         (exit)
            0xD4000001,                   // svc  #0
    };
    const uint64_t total = kCode + sizeof(code) + kMsgLen;

    std::vector<unsigned char> b{0x7f, 'E', 'L', 'F', 2 /*64-bit*/, 1 /*LE*/, 1 /*version*/, 0 /*SYSV*/,
                                 0, 0, 0, 0, 0, 0, 0, 0};
    b.reserve(total);
    put(b, 2, 2);             // e_type ET_EXEC
    put(b, 183, 2);           // e_machine EM_AARCH64
    put(b, 1, 4);             // e_version
    put(b, kBase + kCode, 8);  // e_entry
    put(b, 64, 8);            // e_phoff
    put(b, 0, 8);             // e_shoff
    put(b, 0, 4);             // e_flags
    put(b, 64, 2);            // e_ehsize
    put(b, 56, 2);            // e_phentsize
    put(b, 1, 2);             // e_phnum
    put(b, 64, 2);            // e_shentsize
    put(b, 0, 2);             // e_shnum
    put(b, 0, 2);             // e_shstrndx
    put(b, 1, 4);             // p_type PT_LOAD
    put(b, 5, 4);             // p_flags R+X
    put(b, 0, 8);             // p_offset
    put(b, kBase, 8);         // p_vaddr
    put(b, kBase, 8);         // p_paddr
    put(b, total, 8);         // p_filesz
    put(b, total, 8);         // p_memsz
    put(b, 0x1000, 8);        // p_align
    for (uint32_t insn : code) put(b, insn, 4);
    for (uint32_t i = 0; i < kMsgLen; ++i) b.push_back(static_cast<unsigned char>(kMsg[i]));
    return b;
}

}  // namespace csprobe
