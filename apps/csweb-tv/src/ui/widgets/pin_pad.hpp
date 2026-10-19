// SPDX-License-Identifier: GPL-3.0-or-later
//
// The PIN entry of design 13: a prompt, four dots that fill as digits are entered, and a row of
// digit keys (1-9, 0) with a delete key. The fourth digit submits. Nothing entered is logged or
// shown in clear.
#pragma once

#include "wnk/ui/widgets.hpp"

#include <functional>
#include <string>

namespace csweb::ui {

class PinPad : public wnk::ui::Column {
 public:
    using Submit = std::function<void(wnk::ui::Ui &, const std::string &pin)>;
    explicit PinPad(Submit on_submit);
    void set_prompt(const std::string &text);
    void clear();
    void show_error(const std::string &text);  // replaces the hint, empties the dots
    wnk::ui::View *first_key() const { return first_; }

 private:
    void press(wnk::ui::Ui &ui, char digit);
    void backspace();
    Submit on_submit_;
    std::string entered_;
    wnk::ui::Label *prompt_, *hint_;
    wnk::ui::View *dots_;
    wnk::ui::View *first_ = nullptr;
};

}  // namespace csweb::ui
