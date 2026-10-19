// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wraps a callback so it does nothing once its owner is gone. Runtime and network answers can arrive
// after the screen that asked has closed (a cancelled request still reports done); every screen
// callback goes through this.
#pragma once

#include "wnk/async/async.hpp"

#include <utility>

namespace csweb::core {

template <typename F>
auto guarded(wnk::async::Lifetime::Weak owner, F fn) {
    return [owner, fn = std::move(fn)](auto &&...args) mutable {
        if (owner.alive()) fn(std::forward<decltype(args)>(args)...);
    };
}

}  // namespace csweb::core
