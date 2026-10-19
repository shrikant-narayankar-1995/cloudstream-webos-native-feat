// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player (designs 05 and 06), behaving like CloudStream's GeneratorPlayer:
//   - it asks the provider for the links of what was chosen (loadLinks) and shows them arriving,
//     with "Skip loading (n)" once one is usable;
//   - when loading ends (or the viewer skips, or a link reaches AUTO_SKIP_PRIORITY) it plays the best
//     link (player/link_priority.hpp);
//   - a link that fails is marked and the next one is tried (nextMirror); none left: "No links found";
//   - Sources, Subtitles (the plugin's subtitle files and the stream's own tracks) and Audio are
//     chosen from the controls.
// Playback is the kit's: PlaybackController with the TV's decoder under a transparent UI.
#pragma once

#include "app/router.hpp"
#include "player/subtitle_file.hpp"
#include "runtime/runtime_types.hpp"

#include "wnk/app/wake_lock.hpp"
#include "wnk/media/controller.hpp"
#include "wnk/media/subtitle_view.hpp"
#include "wnk/ui/navigation.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace csweb::ui {

class PillButton;

class PlayerScreen : public wnk::ui::Screen {
 public:
    PlayerScreen(app::Services &s, app::PlayTarget target);
    ~PlayerScreen() override;

    void on_show(wnk::ui::Ui &ui) override;
    void on_hide(wnk::ui::Ui &ui) override;
    bool on_back(wnk::ui::Ui &ui) override;

    // The app went to the background / came back (Application::on_lifecycle).
    void pause_for_background();
    void return_from_background();

 protected:
    bool on_action(wnk::ui::Ui &ui, const wnk::input::ActionEvent &e) override;
    void update(wnk::ui::Ui &ui, float dt) override;

 private:
    void load_links();
    void links_changed();
    void start_best(wnk::ui::Ui &ui);
    void play_link(wnk::ui::Ui &ui, size_t index);
    void next_mirror(wnk::ui::Ui &ui);
    void no_links(const std::string &why);
    void stop_playback();
    void show_controls();
    void open_sources(wnk::ui::Ui &ui);
    void open_subtitles(wnk::ui::Ui &ui);
    void open_audio(wnk::ui::Ui &ui);
    void select_external_subtitle(wnk::ui::Ui &ui, size_t index);
    std::string link_label(const runtime::Link &l) const;

    app::Services &s_;
    app::PlayTarget target_;
    std::vector<runtime::Link> links_;
    std::vector<runtime::Subtitle> subtitles_;
    std::set<size_t> errored_;
    runtime::RuntimeClient::RequestId request_ = 0;
    bool loading_ = true;
    bool started_ = false;   // a link was chosen (GeneratorPlayer.isPlayerActive)
    size_t current_ = SIZE_MAX;
    std::string load_error_;

    std::unique_ptr<wnk::media::PlaybackController> controller_;
    std::vector<wnk::media::TrackInfo> tracks_;
    int external_sub_ = -1;  // index into subtitles_ when a plugin subtitle file is shown
    std::vector<std::shared_ptr<const wnk::media::SubtitleCue>> external_cues_;
    int64_t position_us_ = wnk::media::kNoTime;
    bool resume_on_return_ = false;
    bool next_pending_ = false;  // the playing link failed: try the next one
    wnk::app::WakeLock screen_on_;

    // Loading overlay
    wnk::ui::View *loading_view_ = nullptr;
    wnk::ui::Label *loading_text_ = nullptr;
    PillButton *skip_ = nullptr;
    // Controls
    wnk::media::SubtitleView *subs_view_ = nullptr;
    wnk::ui::Column *bar_ = nullptr;
    wnk::ui::Label *title_ = nullptr, *status_ = nullptr, *time_ = nullptr, *source_ = nullptr;
    wnk::ui::ProgressBar *progress_ = nullptr;
    PillButton *pause_ = nullptr;
    wnk::ui::Spinner *spinner_ = nullptr;
    double last_input_ms_ = 0;
    wnk::async::Lifetime life_;
};

}  // namespace csweb::ui
