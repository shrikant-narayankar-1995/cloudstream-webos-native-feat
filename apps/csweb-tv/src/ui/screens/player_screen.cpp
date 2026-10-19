// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/screens/player_screen.hpp"

#include "core/guard.hpp"
#include "core/redact.hpp"
#include "player/link_priority.hpp"
#include "player/media_factory.hpp"
#include "ui/theme/theme.hpp"
#include "ui/widgets/pill_button.hpp"

#include "wnk/log.hpp"

#include <algorithm>
#include <cstdio>

namespace csweb::ui {

using namespace wnk::ui;
using namespace wnk::media;
using wnk::input::Action;
using wnk::input::Phase;

namespace {
constexpr const char *kCat = "csweb";
constexpr int64_t kSeekUs = 10000000;

std::string clock_text(int64_t us) {
    if (us == kNoTime || us < 0) return "--:--";
    const long long s = us / 1000000;
    char buf[24];
    if (s >= 3600) {
        std::snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60);
    } else {
        std::snprintf(buf, sizeof buf, "%lld:%02lld", s / 60, s % 60);
    }
    return buf;
}

// Host only, for logs: a link's URL may carry tokens.
std::string host_of(const std::string &url) {
    const size_t start = url.find("://");
    if (start == std::string::npos) return "?";
    const size_t end = url.find_first_of("/?#", start + 3);
    std::string host = url.substr(start + 3, end == std::string::npos ? std::string::npos : end - start - 3);
    if (const size_t at = host.rfind('@'); at != std::string::npos) host.erase(0, at + 1);
    return host;
}

std::string track_name(const TrackInfo &t) {
    std::string name = !t.title.empty() ? t.title : !t.language.empty() ? t.language : "Track " + std::to_string(t.index);
    if (!t.codec_label.empty()) name += " (" + t.codec_label + ")";
    return name;
}
}  // namespace

PlayerScreen::PlayerScreen(app::Services &s, app::PlayTarget target)
    : Screen(target.title), s_(s), target_(std::move(target)) {
    name = "player";
    subs_view_ = &add<SubtitleView>();
    subs_view_->set_width(Dim::fill()).set_height(Dim::fill());
    subs_view_->set_provider([this]() -> SubtitleView::Cues {
        if (external_sub_ >= 0) return player::cues_at(external_cues_, position_us_);
        return controller_ ? controller_->subtitles_now() : SubtitleView::Cues{};
    });

    // Controls: a dark band at the bottom.
    bar_ = &add<Column>(16);
    bar_->set_align(Align::Stretch, Align::End);
    bar_->set_padding({theme::kSafeX, 40, theme::kSafeX, 56});
    bar_->set_background(theme::Color::hex(0x05060AD0));
    auto &top = bar_->add<wnk::ui::Row>(24);
    auto &titles = top.add<Column>(6);
    titles.set_width(Dim::fill());
    title_ = &titles.add<Label>(target_.title, TextRole::Title);
    title_->set_bold(true);
    auto &sub = titles.add<Label>(target_.subtitle);
    sub.set_color(theme::kTextSecondary);
    sub.set_visible(!target_.subtitle.empty());
    status_ = &top.add<Label>("");
    status_->set_muted(true).set_align(Align::End, Align::End);
    progress_ = &bar_->add<ProgressBar>();
    auto &times = bar_->add<wnk::ui::Row>(24);
    time_ = &times.add<Label>("--:-- / --:--");
    time_->set_width(Dim::fill());
    source_ = &times.add<Label>("", TextRole::Small);
    source_->set_muted(true);
    auto &buttons = bar_->add<wnk::ui::Row>(16);
    buttons.set_focus_scope();
    buttons.add<PillButton>("", "replay", PillButton::Style::Glass, [this](Ui &) {
        if (controller_) controller_->seek_relative(-kSeekUs);
    }, 64).accessible_label = "Back 10 seconds";
    pause_ = &buttons.add<PillButton>("", "pause", PillButton::Style::Glass, [this](Ui &) {
        if (controller_) controller_->toggle_pause();
    }, 64);
    pause_->accessible_label = "Play or pause";
    pause_->name = "player.pause";
    buttons.add<PillButton>("", "skip", PillButton::Style::Glass, [this](Ui &) {
        if (controller_) controller_->seek_relative(kSeekUs);
    }, 64).accessible_label = "Forward 10 seconds";
    auto &src = buttons.add<PillButton>("Sources", "play", PillButton::Style::Glass, [this](Ui &ui) { open_sources(ui); }, 64);
    src.name = "player.sources";
    buttons.add<PillButton>("Subtitles", "subtitles", PillButton::Style::Glass, [this](Ui &ui) { open_subtitles(ui); }, 64)
        .name = "player.subtitles";
    buttons.add<PillButton>("Audio", "audio", PillButton::Style::Glass, [this](Ui &ui) { open_audio(ui); }, 64).name =
        "player.audio";
    next_ = &buttons.add<PillButton>("Next episode", "skip", PillButton::Style::Glass, [this](Ui &ui) { play_next(ui); }, 64);
    next_->name = "player.next";
    next_->set_visible(!target_.queue.empty());
    bar_->set_visible(false);

    spinner_ = &add<Spinner>(96);
    spinner_->set_align(Align::Center, Align::Center);
    spinner_->set_visible(false);

    // Loading overlay (CloudStream's link loading screen, with Skip loading).
    auto &loading = add<Column>(26);
    loading_view_ = &loading;
    loading.set_width(Dim::fill()).set_height(Dim::fill());
    loading.set_background(theme::kBackground);
    loading.set_padding({theme::kSafeX, 360, theme::kSafeX, 80});
    auto &lt = loading.add<Label>(target_.title, TextRole::Title);
    lt.set_font_size(theme::kDisplay).set_bold(true).set_max_lines(2);
    loading.add<Label>(target_.subtitle).set_color(theme::kTextSecondary);
    loading_text_ = &loading.add<Label>("Loading links…");
    loading_text_->set_muted(true).set_max_lines(3);
    auto &lrow = loading.add<wnk::ui::Row>(18);
    lrow.add<Spinner>(56);
    skip_ = &lrow.add<PillButton>("Skip loading", "skip", PillButton::Style::Primary, [this](Ui &ui) { start_best(ui); }, 64);
    skip_->name = "player.skip";
    skip_->set_visible(false);
}

PlayerScreen::~PlayerScreen() {
    if (request_ != 0) s_.runtime->cancel(request_);
    save_position();
    stop_playback();
}

void PlayerScreen::on_show(Ui &ui) {
    Screen::on_show(ui);
    if (request_ == 0 && links_.empty() && loading_) load_links();
    if (controller_) ui.set_clear_color(wnk::render::kTransparent);
}

void PlayerScreen::on_hide(Ui &ui) {
    ui.set_clear_color(std::nullopt);
    Screen::on_hide(ui);
}

bool PlayerScreen::on_back(Ui &) {
    save_position();
    stop_playback();
    return false;
}

void PlayerScreen::load_links() {
    loading_ = true;
    request_ = s_.runtime->request(
        {{"op", "links"}, {"provider", target_.provider}, {"url", target_.data}, {"withHeaders", true}},
        core::guarded(life_.weak(), [this](const nlohmann::json &e) {
            const std::string ev = e.value("event", "");
            if (ev == "link") {
                links_.push_back(runtime::parse_link(e));
                const auto &l = links_.back();
                WNK_LOGD(kCat, "link: %s %s %s from %s", l.name.c_str(), runtime::quality_label(l.quality).c_str(),
                         l.link_type.c_str(), host_of(l.url).c_str());
                links_changed();
            } else if (ev == "subtitle") {
                subtitles_.push_back(runtime::parse_subtitle(e));
            }
        }),
        core::guarded(life_.weak(), [this](bool ok, const std::string &error) {
            request_ = 0;
            loading_ = false;
            if (!ok) load_error_ = error;
            WNK_LOGI(kCat, "links loaded: %zu links, %zu subtitles%s%s", links_.size(), subtitles_.size(),
                     ok ? "" : ", error: ", error.c_str());
            // GeneratorPlayer: Success or Failure both start the player (Failure with a toast).
            if (Ui *u = this->ui()) {
                if (!ok && !error.empty() && error != "Cancelled") u->toast(error, 4000);
                if (!started_) start_best(*u);
            }
        }));
}

void PlayerScreen::links_changed() {
    size_t usable = 0;
    bool auto_skip = false;
    for (const auto &l : links_) {
        if (!player::playable(l)) continue;
        ++usable;
        if (player::link_priority(l) >= player::kAutoSkipPriority) auto_skip = true;
    }
    loading_text_->set_text(links_.empty() ? "Loading links…" : "Loading links… " + std::to_string(links_.size()) + " found");
    if (usable > 0) {
        const bool was_hidden = !skip_->visible();
        skip_->set_text("Skip loading (" + std::to_string(usable) + ")");
        skip_->set_visible(true);
        if (was_hidden) {
            if (Ui *u = this->ui()) u->focus(skip_);
        }
    }
    if (!started_ && auto_skip) {
        if (Ui *u = this->ui()) start_best(*u);
    }
}

void PlayerScreen::start_best(Ui &ui) {
    if (started_) return;
    for (size_t i : player::sort_links(links_)) {
        if (player::playable(links_[i]) && !errored_.count(i)) {
            started_ = true;
            play_link(ui, i);
            return;
        }
    }
    if (!loading_) no_links(links_.empty() ? "No links found" : "No playable links found");
}

void PlayerScreen::no_links(const std::string &why) {
    stop_playback();
    loading_view_->set_visible(true);
    skip_->set_visible(false);
    loading_text_->set_text(why + (load_error_.empty() ? std::string() : " (" + load_error_ + ")") +
                            (links_.empty() ? "" : ". Torrent links cannot be played on the TV."));
    if (Ui *u = this->ui()) u->set_clear_color(std::nullopt);
}

void PlayerScreen::stop_playback() {
    if (controller_) {
        controller_->stop();
        controller_.reset();
    }
    if (screen_on_.held()) screen_on_.release();
}

void PlayerScreen::play_link(Ui &ui, size_t index) {
    stop_playback();
    current_ = index;
    const runtime::Link &link = links_[index];
    WNK_LOGI(kCat, "playing link %zu: %s %s (%s)", index, link.name.c_str(), runtime::quality_label(link.quality).c_str(),
             player::is_adaptive(link) ? "adaptive" : "ffmpeg");
    tracks_.clear();
    loading_view_->set_visible(false);
    bar_->set_visible(true);
    source_->set_text(link_label(link));
    ui.set_clear_color(wnk::render::kTransparent);
    auto http = s_.ctx->http();
    controller_ = std::make_unique<PlaybackController>(player::make_source(link, http, s_.device), player::make_backend(s_.device),
                                                       s_.ctx->dispatcher(), PlaybackOptions{});
    controller_->set_state_callback(core::guarded(life_.weak(), [this](PlayState from, PlayState to, const PlaybackSnapshot &snap) {
        const std::string error = core::redact_queries(snap.error);
        WNK_LOGI(kCat, "player %s -> %s%s%s", play_state_name(from), play_state_name(to), error.empty() ? "" : ": ",
                 error.c_str());
        if (to == PlayState::Ready) {
            controller_->set_video_area({0, 0, 1920, 1080});
            // GeneratorPlayer.getPos: continue where the viewer stopped (not past 95 %).
            if (!resumed_) {
                resumed_ = true;
                const int64_t start = s_.library->get().start_position_ms(target_.provider, target_.data);
                if (start > 0) {
                    WNK_LOGI(kCat, "player: resuming at %lld s", static_cast<long long>(start / 1000));
                    controller_->seek(start * 1000);
                }
            }
            tracks_ = snap.info.tracks;
            for (const TrackInfo &t : tracks_) {
                if (t.type == TrackType::Subtitle && t.subtitle != SubtitleKind::None && t.is_default) {
                    controller_->set_subtitle_track(t.index);
                    break;
                }
            }
        } else if (to == PlayState::Ended) {
            ended_pending_ = true;
        } else if (to == PlayState::Paused) {
            save_position();
        } else if (to == PlayState::Error) {
            // The next link is opened from update(): this controller must not be destroyed inside
            // its own callback.
            errored_.insert(current_);
            next_pending_ = true;
            if (Ui *u = this->ui()) u->toast("This link failed: " + (error.empty() ? std::string("playback error") : error), 3500);
        }
        show_controls();
    }));
    controller_->set_video_area({0, 0, 1920, 1080});
    controller_->open(link.url);
    show_controls();
}

void PlayerScreen::next_mirror(Ui &ui) {
    // GeneratorPlayer.getNextLink: the next link after the current one in sorted order.
    const auto order = player::sort_links(links_);
    auto it = std::find(order.begin(), order.end(), current_);
    for (++it; it < order.end(); ++it) {
        if (player::playable(links_[*it]) && !errored_.count(*it)) {
            play_link(ui, *it);
            return;
        }
    }
    no_links("No more links to try");
}

std::string PlayerScreen::link_label(const runtime::Link &l) const {
    std::string label = l.name.empty() ? l.source : l.name;
    const std::string q = runtime::quality_label(l.quality);
    if (!q.empty()) label += "  ·  " + q;
    return label;
}

void PlayerScreen::show_controls() {
    if (Ui *u = this->ui()) last_input_ms_ = u->now_ms();
}

bool PlayerScreen::on_action(Ui &ui, const wnk::input::ActionEvent &e) {
    if (!controller_ || e.phase == Phase::Release) return false;
    const bool hidden = bar_->opacity() < 0.5f;
    show_controls();
    switch (e.action) {
        case Action::PlayPause:
            if (e.phase == Phase::Press) controller_->toggle_pause();
            return true;
        case Action::Play: controller_->play(); return true;
        case Action::Pause: controller_->pause(); return true;
        case Action::Rewind: controller_->seek_relative(-kSeekUs); return true;
        case Action::FastForward: controller_->seek_relative(kSeekUs); return true;
        case Action::Confirm:
            if (!hidden) return false;
            if (e.phase == Phase::Press) controller_->toggle_pause();
            return true;
        case Action::Left:
            if (!hidden) return false;
            controller_->seek_relative(-kSeekUs);
            return true;
        case Action::Right:
            if (!hidden) return false;
            controller_->seek_relative(kSeekUs);
            return true;
        case Action::Up:
        case Action::Down:
            if (!hidden) return false;
            ui.focus(pause_);
            return true;
        default: return false;
    }
}

void PlayerScreen::update(Ui &ui, float dt) {
    Screen::update(ui, dt);
    if (next_pending_) {
        next_pending_ = false;
        next_mirror(ui);
    }
    if (ended_pending_) {
        ended_pending_ = false;
        if (duration_us_ != kNoTime) position_us_ = duration_us_;
        save_position();
        if (!target_.queue.empty()) {
            play_next(ui);
            return;
        }
    }
    if (!controller_) return;
    const PlaybackSnapshot s = controller_->snapshot();
    position_us_ = s.position_us;
    if (s.duration_us > 0) duration_us_ = s.duration_us;
    if (s.state == PlayState::Playing && ui.now_ms() - last_save_ms_ > 10000) {
        last_save_ms_ = ui.now_ms();
        save_position();
    }
    const bool busy = s.state == PlayState::Opening || s.state == PlayState::Seeking;
    if (busy || s.state == PlayState::Playing) {
        if (!screen_on_.held()) screen_on_ = s_.ctx->keep_screen_on("video playing");
    } else if (screen_on_.held()) {
        screen_on_.release();
    }
    spinner_->set_visible(busy);
    status_->set_text(s.state == PlayState::Playing ? "" : play_state_name(s.state));
    pause_->set_icon(s.state == PlayState::Paused ? "play" : "pause");
    const bool live = s.duration_us == kNoTime && (s.info.container == "dash" || s.info.container == "hls");
    time_->set_text(clock_text(s.position_us) + " / " + (live ? std::string("LIVE") : clock_text(s.duration_us)));
    if (s.duration_us > 0 && s.position_us != kNoTime) progress_->set_value(static_cast<float>(s.position_us) / s.duration_us);
    const bool keep = s.state != PlayState::Playing || ui.top_dialog() != nullptr;
    const bool visible = keep || ui.now_ms() - last_input_ms_ < 5000;
    bar_->set_opacity(visible ? 1.0f : 0.0f);
    subs_view_->set_bottom_margin(visible ? 0.34f : 0.07f);
}

void PlayerScreen::open_sources(Ui &ui) {
    auto d = std::make_unique<Dialog>("Sources", loading_ ? "Still loading links…" : std::string());
    Dialog *dlg = d.get();
    auto &scroll = d->body().add<ScrollView>();
    scroll.set_height(Dim::fixed(560));
    auto &list = scroll.add<Column>(8);
    View *current = nullptr;
    for (size_t i : player::sort_links(links_)) {
        const auto &l = links_[i];
        std::string text = link_label(l);
        if (!player::playable(l)) text += "  ·  not supported";
        else if (errored_.count(i)) text += "  ·  failed";
        auto &b = list.add<PillButton>(text, i == current_ ? "check" : "", PillButton::Style::Glass, [this, dlg, i](Ui &u) {
            dlg->dismiss(u);
            if (player::playable(links_[i])) {
                errored_.erase(i);
                started_ = true;
                play_link(u, i);
            }
        }, 60);
        b.name = "sources." + std::to_string(i);
        if (i == current_) current = &b;
    }
    d->add_button("Close");
    ui.show_dialog(std::move(d));
    if (current) ui.focus(current);
}

void PlayerScreen::open_subtitles(Ui &ui) {
    auto d = std::make_unique<Dialog>("Subtitles");
    Dialog *dlg = d.get();
    auto &scroll = d->body().add<ScrollView>();
    scroll.set_height(Dim::fixed(560));
    auto &list = scroll.add<Column>(8);
    const int embedded = controller_ ? controller_->subtitle_track() : -1;
    list.add<PillButton>("Off", external_sub_ < 0 && embedded < 0 ? "check" : "", PillButton::Style::Glass, [this, dlg](Ui &u) {
        external_sub_ = -1;
        if (controller_) controller_->set_subtitle_track(-1);
        dlg->dismiss(u);
    }, 60);
    for (size_t i = 0; i < subtitles_.size(); i++) {
        const auto &sub = subtitles_[i];
        list.add<PillButton>(sub.lang.empty() ? "Subtitle " + std::to_string(i + 1) : sub.lang,
                             external_sub_ == static_cast<int>(i) ? "check" : "", PillButton::Style::Glass,
                             [this, dlg, i](Ui &u) {
                                 dlg->dismiss(u);
                                 select_external_subtitle(u, i);
                             }, 60)
            .name = "subs.file." + std::to_string(i);
    }
    for (const TrackInfo &t : tracks_) {
        if (t.type != TrackType::Subtitle || t.subtitle == SubtitleKind::None) continue;
        const int index = t.index;
        list.add<PillButton>(track_name(t), external_sub_ < 0 && embedded == index ? "check" : "", PillButton::Style::Glass,
                             [this, dlg, index](Ui &u) {
                                 external_sub_ = -1;
                                 if (controller_) controller_->set_subtitle_track(index);
                                 dlg->dismiss(u);
                             }, 60)
            .name = "subs.track." + std::to_string(index);
    }
    d->add_button("Close");
    ui.show_dialog(std::move(d));
}

void PlayerScreen::select_external_subtitle(Ui &ui, size_t index) {
    if (index >= subtitles_.size() || !s_.http) return;
    wnk::net::HttpRequest req;
    req.url = subtitles_[index].url;
    req.max_response_bytes = 8u << 20;
    req.timeout_ms = 30000;
    req.headers = {{"User-Agent", player::kCloudStreamUserAgent}};
    ui.toast("Loading subtitles…", 1500);
    s_.http->send(req, {}, life_.weak(), [this, index](wnk::net::HttpResponse res) {
        Ui *u = this->ui();
        if (!res.ok()) {
            if (u) u->toast("Subtitles could not be downloaded.");
            return;
        }
        std::vector<SubtitleCue> cues;
        std::string error;
        if (!player::parse_subtitle_file(res.body, &cues, &error)) {
            WNK_LOGW(kCat, "subtitle file %zu: %s", index, error.c_str());
            if (u) u->toast("This subtitle format is not supported.");
            return;
        }
        external_cues_.clear();
        for (auto &c : cues) external_cues_.push_back(std::make_shared<const SubtitleCue>(std::move(c)));
        external_sub_ = static_cast<int>(index);
        if (controller_) controller_->set_subtitle_track(-1);
        WNK_LOGI(kCat, "subtitle file %zu: %zu cues", index, external_cues_.size());
    });
}

void PlayerScreen::open_audio(Ui &ui) {
    if (!controller_) return;
    const PlaybackSnapshot snap = controller_->snapshot();
    auto d = std::make_unique<Dialog>("Audio");
    Dialog *dlg = d.get();
    size_t count = 0;
    for (const TrackInfo &t : tracks_) {
        if (t.type != TrackType::Audio) continue;
        ++count;
        const int index = t.index;
        d->body().add<PillButton>(track_name(t), index == snap.audio_track ? "check" : "", PillButton::Style::Glass,
                                  [this, dlg, index](Ui &u) {
                                      if (controller_) controller_->set_audio_track(index);
                                      dlg->dismiss(u);
                                  }, 60);
    }
    if (count == 0) d->body().add<Label>("No audio tracks to choose from.").set_muted(true);
    d->add_button("Close");
    ui.show_dialog(std::move(d));
}

void PlayerScreen::save_position() {
    // Only real progress of a known length (live streams have none).
    if (!started_ || position_us_ == kNoTime || duration_us_ == kNoTime || duration_us_ <= 0 || position_us_ < 0) return;
    if (target_.show.url.empty()) return;
    library::EpisodeRef cur{target_.data, target_.subtitle, target_.season, target_.episode};
    std::optional<library::EpisodeRef> next;
    if (!target_.queue.empty()) next = target_.queue.front();
    const int64_t pos = position_us_ / 1000, dur = duration_us_ / 1000;
    s_.library->update([&](library::Library &lib) {
        lib.set_view_pos_and_resume(target_.show, cur, next, pos, dur, library::now_ms());
    });
}

void PlayerScreen::play_next(Ui &ui) {
    if (target_.queue.empty()) return;
    save_position();
    app::PlayTarget t = target_;
    const library::EpisodeRef n = t.queue.front();
    t.queue.erase(t.queue.begin());
    t.data = n.data;
    t.subtitle = n.name;
    t.season = n.season;
    t.episode = n.episode;
    WNK_LOGI(kCat, "player: next episode");
    s_.router->replace_player(ui, t);
}

void PlayerScreen::pause_for_background() {
    save_position();
    if (!controller_) return;
    const PlayState st = controller_->state();
    resume_on_return_ = st == PlayState::Playing || st == PlayState::Seeking;
    controller_->pause_and_release();
}

void PlayerScreen::return_from_background() {
    if (!controller_ || !resume_on_return_) return;
    resume_on_return_ = false;
    controller_->play();
}

}  // namespace csweb::ui
