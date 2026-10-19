// SPDX-License-Identifier: GPL-3.0-or-later
//
// A main-thread notification list: owners subscribe a callback and keep the returned Subscription;
// destroying it unsubscribes (a screen that closes stops listening by itself).
#pragma once

#include <functional>
#include <map>
#include <memory>

namespace csweb::core {

class Signal {
 public:
    class Subscription {
     public:
        Subscription() = default;
        Subscription(Subscription &&) = default;
        Subscription &operator=(Subscription &&) = default;
        ~Subscription() {
            if (auto m = map_.lock()) m->erase(id_);
        }

     private:
        friend class Signal;
        Subscription(std::weak_ptr<std::map<int, std::function<void()>>> m, int id) : map_(std::move(m)), id_(id) {}
        std::weak_ptr<std::map<int, std::function<void()>>> map_;
        int id_ = 0;
    };

    Subscription subscribe(std::function<void()> fn) {
        const int id = ++next_;
        (*map_)[id] = std::move(fn);
        return Subscription(map_, id);
    }
    void emit() {
        auto copy = *map_;  // a callback may unsubscribe (close its screen)
        for (auto &kv : copy) {
            if (map_->count(kv.first)) kv.second();
        }
    }

 private:
    std::shared_ptr<std::map<int, std::function<void()>>> map_ = std::make_shared<std::map<int, std::function<void()>>>();
    int next_ = 0;
};

}  // namespace csweb::core
