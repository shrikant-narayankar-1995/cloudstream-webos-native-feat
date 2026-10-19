// SPDX-License-Identifier: GPL-3.0-or-later
// core::Sha256 against the FIPS 180-4 / NIST example vectors.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "core/sha256.hpp"

using csweb::core::Sha256;

TEST("empty_and_abc") {
    CHECK_EQ(Sha256::hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(Sha256::hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST("two_blocks") {
    CHECK_EQ(Sha256::hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST("million_a_in_pieces") {
    Sha256 h;
    const std::string chunk(997, 'a');
    size_t left = 1000000;
    while (left > 0) {
        const size_t n = left < chunk.size() ? left : chunk.size();
        h.update(chunk.data(), n);
        left -= n;
    }
    CHECK_EQ(h.finish_hex(), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST("hex_check") {
    CHECK(csweb::core::is_sha256_hex(std::string(64, 'a')));
    CHECK(!csweb::core::is_sha256_hex(std::string(63, 'a')));
    CHECK(!csweb::core::is_sha256_hex(std::string(64, 'g')));
}
