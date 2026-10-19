// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/screens/profiles_screen.hpp"

#include "profiles/profiles.hpp"
#include "ui/theme/theme.hpp"
#include "ui/widgets/pill_button.hpp"

#include "wnk/log.hpp"

namespace csweb::ui {

using namespace wnk::ui;

namespace {
constexpr const char *kCat = "csweb";

void prompt_pin(Ui &ui, const std::string &title, const std::string &message,
                std::function<void(Ui &, const std::string &)> done) {
    auto d = std::make_unique<Dialog>(title, message);
    Dialog *dlg = d.get();
    auto &field = d->body().add<TextField>("4 digits");
    field.set_password(true).set_max_length(4);
    field.set_width(Dim::fixed(500));
    field.name = "profiles.pin";
    TextField *f = &field;
    field.set_on_submit([done, dlg](Ui &u, const std::string &pin) {
        dlg->dismiss(u);
        done(u, pin);
    });
    d->add_button("Cancel");
    d->add_button("OK", [done, f](Ui &u) { done(u, f->text()); }).name = "profiles.pin.ok";
    ui.show_dialog(std::move(d));
    ui.focus(f);
}
}  // namespace

void unlock_profile(Ui &ui, app::Services &s, int key_index, std::function<void(Ui &)> then) {
    const profiles::Profile *p = s.profiles.find(key_index);
    if (p == nullptr) return;
    if (!p->locked()) {
        then(ui);
        return;
    }
    const std::string name = p->name;
    prompt_pin(ui, "Enter the PIN for " + name, "", [&s, key_index, then, name](Ui &u, const std::string &pin) {
        if (!s.profiles.check_pin(key_index, pin)) {
            u.toast("Wrong PIN for " + name + ".");  // the PIN itself is never logged or shown
            return;
        }
        then(u);
    });
}

ProfilesScreen::ProfilesScreen(app::Services &s, bool startup, std::function<void(Ui &)> on_chosen)
    : Screen("Profiles"), s_(s), startup_(startup), on_chosen_(std::move(on_chosen)) {
    name = "profiles";
    set_background(theme::kBackground);
    auto &col = add<Column>(40);
    col.set_width(Dim::fill()).set_height(Dim::fill());
    col.set_padding({theme::kSafeX, 220, theme::kSafeX, 80});
    col.set_align(Align::Center, Align::Start);
    col.add<Label>("Who's watching?", TextRole::Title).set_font_size(theme::kDisplay).set_bold(true);
    hint_ = &col.add<Label>("");
    hint_->set_muted(true);
    tiles_ = &col.add<wnk::ui::Row>(48);
    tiles_->set_focus_scope();
    manage_ = &col.add<PillButton>("Manage profiles", "settings", PillButton::Style::Glass, [this](Ui &ui) {
        managing_ = !managing_;
        rebuild();
        if (first_) ui.focus(first_);
    }, 60);
    manage_->name = "profiles.manage";
    rebuild();
}

void ProfilesScreen::on_show(Ui &ui) {
    Screen::on_show(ui);
    if (first_) ui.focus(first_);
}

bool ProfilesScreen::on_back(Ui &ui) {
    if (managing_) {
        managing_ = false;
        rebuild();
        if (first_) ui.focus(first_);
        return true;
    }
    if (startup_) {
        // AccountSelectActivity is the first screen: Back leaves instead of skipping the choice.
        if (s_.ctx) s_.ctx->quit("back on profile selection");
        return true;
    }
    return false;
}

void ProfilesScreen::save() { s_.settings->set(settings::kProfiles, s_.profiles.to_json()); }

void ProfilesScreen::rebuild() {
    tiles_->clear_children();
    first_ = nullptr;
    hint_->set_text(managing_ ? "Choose a profile to change it." : "Each profile has its own library, home source and searches.");
    manage_->set_text(managing_ ? "Done" : "Manage profiles");
    for (const auto &p : s_.profiles.all()) {
        const int key = p.key_index;
        auto &tile = tiles_->add<Column>(18);
        tile.set_align(Align::Center, Align::Start);
        auto &avatar = tile.add<Label>(profiles::initials(p.name), TextRole::Title);
        avatar.set_bold(true).set_text_align(Align::Center);
        avatar.set_size(180, 180).set_background(theme::avatar_colour(p.image_index), 90);
        avatar.set_padding({0, 60, 0, 0});
        const bool current = key == s_.profiles.selected();
        auto &b = tile.add<PillButton>(p.name, managing_ ? "settings" : p.locked() ? "lock" : current ? "check" : "",
                                       PillButton::Style::Glass, [this, key](Ui &ui) {
                                           if (managing_) edit(ui, key);
                                           else choose(ui, key);
                                       }, 60);
        b.name = "profiles.profile." + std::to_string(key);
        if (first_ == nullptr || current) first_ = &b;
    }
    if (managing_) {
        auto &tile = tiles_->add<Column>(18);
        tile.set_align(Align::Center, Align::Start);
        auto &avatar = tile.add<Label>("+", TextRole::Title);
        avatar.set_bold(true).set_text_align(Align::Center);
        avatar.set_size(180, 180).set_background(theme::Color::hex(0x2A3040FF), 90);
        avatar.set_padding({0, 60, 0, 0});
        tile.add<PillButton>("Add profile", "plus", PillButton::Style::Glass, [this](Ui &ui) {
            ask_name(ui, "New profile", "", [this](Ui &u, const std::string &name) {
                const int key = s_.profiles.add(name).key_index;
                save();
                WNK_LOGI(kCat, "profiles: added %d", key);
                rebuild();
                if (first_) u.focus(first_);
            });
        }, 60).name = "profiles.add";
    }
}

void ProfilesScreen::choose(Ui &ui, int key_index) {
    unlock_profile(ui, s_, key_index, [this, key_index](Ui &u) {
        auto chosen = on_chosen_;
        const bool startup = startup_;
        s_.router->switch_profile(u, key_index);  // pops this screen
        if (startup && chosen) chosen(u);
    });
}

void ProfilesScreen::edit(Ui &ui, int key_index) {
    const profiles::Profile *p = s_.profiles.find(key_index);
    if (p == nullptr) return;
    auto d = std::make_unique<Dialog>(p->name);
    Dialog *dlg = d.get();
    auto add = [&](const std::string &text, const std::string &icon, std::function<void(Ui &)> fn, const std::string &id) {
        d->body().add<PillButton>(text, icon, PillButton::Style::Glass, [dlg, fn](Ui &u) {
            dlg->dismiss(u);
            fn(u);
        }, 60).name = "profiles.edit." + id;
    };
    add("Rename", "", [this, key_index](Ui &u) {
        const profiles::Profile *q = s_.profiles.find(key_index);
        ask_name(u, "Rename profile", q ? q->name : "", [this, key_index](Ui &v, const std::string &name) {
            if (s_.profiles.rename(key_index, name)) save();
            rebuild();
            if (first_) v.focus(first_);
        });
    }, "rename");
    // Changing or removing a PIN asks for the current one first.
    add(p->locked() ? "Change PIN" : "Set a PIN", "lock", [this, key_index](Ui &u) {
        unlock_profile(u, s_, key_index, [this, key_index](Ui &v) {
            ask_pin(v, "New PIN", [this, key_index](Ui &w, const std::string &pin) {
                if (!s_.profiles.set_pin(key_index, pin)) {
                    w.toast("A PIN is four digits.");
                    return;
                }
                save();
                rebuild();
                w.toast("PIN set.");
            });
        });
    }, "pin");
    if (p->locked()) {
        add("Remove PIN", "", [this, key_index](Ui &u) {
            unlock_profile(u, s_, key_index, [this, key_index](Ui &v) {
                s_.profiles.set_pin(key_index, "");
                save();
                rebuild();
                v.toast("PIN removed.");
            });
        }, "unpin");
    }
    if (key_index != 0) {
        add("Delete profile", "trash", [this, key_index](Ui &u) {
            unlock_profile(u, s_, key_index, [this, key_index](Ui &v) {
                const bool was_current = s_.profiles.selected() == key_index;
                if (was_current) s_.router->switch_profile(v, 0, false);  // stay here
                s_.profiles.remove(key_index);
                save();
                s_.settings->erase_profile(key_index);
                s_.library->erase_profile(key_index);
                WNK_LOGI(kCat, "profiles: deleted %d", key_index);
                rebuild();
                if (first_) v.focus(first_);
            });
        }, "delete");
    }
    d->add_button("Close");
    ui.show_dialog(std::move(d));
}

void ProfilesScreen::ask_name(Ui &ui, const std::string &title, const std::string &current,
                              std::function<void(Ui &, const std::string &)> done) {
    auto d = std::make_unique<Dialog>(title);
    Dialog *dlg = d.get();
    auto &field = d->body().add<TextField>("Name");
    field.set_max_length(40);
    field.set_text(current);
    field.set_width(Dim::fixed(700));
    field.name = "profiles.name";
    TextField *f = &field;
    field.set_on_submit([done, dlg](Ui &u, const std::string &name) {
        dlg->dismiss(u);
        done(u, name);
    });
    d->add_button("Cancel");
    d->add_button("OK", [done, f](Ui &u) { done(u, f->text()); }).name = "profiles.name.ok";
    ui.show_dialog(std::move(d));
    ui.focus(f);
}

void ProfilesScreen::ask_pin(Ui &ui, const std::string &title, std::function<void(Ui &, const std::string &)> done) {
    prompt_pin(ui, title, "Four digits, asked when this profile is chosen.", std::move(done));
}

}  // namespace csweb::ui
