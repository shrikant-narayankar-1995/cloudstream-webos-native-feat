// SPDX-License-Identifier: GPL-3.0-or-later
//
// SHA-256 (FIPS 180-4). Plugins are identified by the hash of their bytes: it is recorded when a
// plugin is installed, checked when the stored copy is written out for the runtime, and passed to the
// runtime, which checks it again before loading (runtime/csloader --sha256).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace csweb::core {

class Sha256 {
 public:
    Sha256();
    void update(const void *data, size_t size);
    void update(std::string_view data) { update(data.data(), data.size()); }
    std::array<uint8_t, 32> finish();
    std::string finish_hex();

    static std::string hex(std::string_view data);

 private:
    void block(const uint8_t *p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t buf_len_ = 0;
    uint64_t total_ = 0;
};

// True for 64 lowercase or uppercase hex digits.
bool is_sha256_hex(std::string_view s);

}  // namespace csweb::core
