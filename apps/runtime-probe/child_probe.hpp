// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

namespace csprobe {

// Prints key=value lines on stdout describing what this process may do (exec mappings, entropy,
// limits, mounts, threads, network). Returns the process exit code.
int run_child_probe(const std::string &app_dir, const std::string &tmp_dir, bool network);

// A minimal static AArch64 Linux executable that writes "a64 ok\n" and exits with status 42.
std::vector<unsigned char> aarch64_exit42_elf();

}  // namespace csprobe
