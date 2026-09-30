// The panel: the application's own window, laid out as Fluent cards.
//
// The default is the Python build's arrangement: one column of groups, scrolled, with the
// status strip pinned at the bottom. Splitting the groups into tabs is offered as a
// setting, for a window that would rather show one job at a time.
#pragma once

#include <optional>
#include <string>

#include "core/config.h"
#include "core/mpv.h"
#include "core/sync.h"
#include "ui/controls.h"
#include "debug.h"
#include "ui/renderer.h"

namespace sp::app {

enum class Tab { Sources = 0, Sync, Windows, Settings, Count };
constexpr int kTabCount = static_cast<int>(Tab::Count);

class Panel {
 public:
  // The smallest window that shows every tab without clipping: each tab declares what
  // it needs and this is the largest of them.
  // While playing, a frame every 150ms is smooth enough for the timeline and costs a
  // fifth of what the 33ms cadence did. When nothing is changing, no frame is drawn.
  static constexpr double kPlayingRepaintSecs = 0.15;

  static constexpr float kMinWidth = 420.0f;
  static constexpr float kMinHeight = 560.0f;

  bool init(HWND hwnd);
  void shutdown();

  // called from the window procedure
  // Is this client point (in physical pixels, as the window sees it) a drag handle? The
  // panel answers because it owns the title bar and button rects in design units, and knows the
  // divisor they were laid out with. Points over a title-bar button are not drag handles.
  bool wants_caption(int client_x, int client_y) const {
    const float s = (input_scale_ > 0.0f) ? input_scale_ : 1.0f;
    const float dx = static_cast<float>(client_x) / s;
    const float dy = static_cast<float>(client_y) / s;
    if (!title_bar_.contains(dx, dy)) return false;
    if (btn_settings_.contains(dx, dy)) return false;
    if (btn_min_.contains(dx, dy)) return false;
    if (btn_close_.contains(dx, dy)) return false;
    return true;
  }

  void on_input(const ui::InputState& in) {
    in_ = in;
    // Mouse messages carry physical pixels, but every rect here is in design units. The
    // divisor is the one the layout recorded when it placed those rects (input_scale_),
    // deliberately not recomputed here: if scale_dpi_ or scale_ changes between the layout and
    // the next mouse message, recomputing would put the pointer in a different space from the
    // rects, which shows up as controls that do not answer where they are drawn.
    const float s = (input_scale_ > 0.0f) ? input_scale_ : 1.0f;
    in_.mouse_x = in.mouse_x / s;
    in_.mouse_y = in.mouse_y / s;
    if (sp::app::debug_on()) {
      sp::app::dbg("raw", static_cast<long>(in.mouse_x), static_cast<long>(in.mouse_y));
      sp::app::dbg("dip", static_cast<long>(in_.mouse_x), static_cast<long>(in_.mouse_y));
      sp::app::dbg("scale", static_cast<long>(scale_dpi_ * 1000.0f),
                   static_cast<long>(scale_ * 1000.0f));
      sp::app::dbg("iscale", static_cast<long>(input_scale_ * 1000.0f),
                   static_cast<long>(design_w()));
    }
  }
  void on_resize(float w, float h);
  // Design units are device-independent pixels: the client size divided by the display
  // scale, so the interface keeps its proportions on a scaled display.
  float design_w() const { return width_ / (scale_dpi_ * scale_); }
  float design_h() const { return height_ / (scale_dpi_ * scale_); }
  void on_dpi(float dpi);
  void draw();
  void tick();  // the sync loop, roughly every 33 ms

  // True when something the panel shows has changed and it needs a frame. The window
  // only repaints when this says so, which is what keeps an idle panel at no cost.
  bool wants_repaint() const { return dirty_; }

  // How often the sync loop needs to run, in milliseconds; 0 means it does not need to
  // run at all. Nothing loaded needs no waking, paused needs only enough to notice a
  // seek, and playing needs the full cadence.
  int tick_interval_ms() const {
    if (!started_) return 0;
    return playing_ ? 33 : 250;
  }

  void remember_window(int x, int y, int w, int h);

  // Mouse wheel, in multiples of WHEEL_DELTA, for the scrolled column.
  void on_wheel(int delta);

  // The smallest height that suits the current arrangement: a stacked column scrolls, so
  // it needs far less room than a tabbed one, which must show a whole tab.
  int min_height() const;

  // True while a text field has focus, so the window's shortcuts stay out of the way of
  // typing.
  bool editing_text() const;

  // ---- actions (public: the window procedure drives some of them) ----------
  void start_sources();
  void stop_players();
  void toggle_play();
  void nudge_jump(int direction);
  void select_tab(int index);
  int current_tab() const { return static_cast<int>(tab_); }

 private:
  struct SourceRow {
    ui::RectF label, field, browse;
  };
  struct TimelineRow {
    ui::RectF label, bar, time;
    ui::RectF play;  // the per-video play/pause button (rows 0 and 1 only)
  };

  // ---- actions -------------------------------------------------------------
  void seek_side(Side side, double seconds);
  void toggle_play_side(Side side);
  void seek_master(double seconds);
  void apply_offset_from_field();
  void apply_speed_from_field();
  void apply_volume();
  void arrange_windows();
  void toggle_floating_pip();
  void browse_for(Side side);
  void sync_tick();
  void update_timer();
  void set_message(const std::wstring& text);
  void set_dark(bool dark);
  double jump_seconds() const;
  std::optional<double> parse_timecode(const std::wstring& text) const;
  // Formatted once per change of the value behind it, so a playing frame draws
  // from the cached string instead of rebuilding it with fmt/widen.
  const std::wstring& offset_label();
  const std::wstring& jump_back_label();
  const std::wstring& jump_fwd_label();
  const std::wstring& time_label(unsigned row, double pos, double dur);
  const std::wstring& vol_label(unsigned i);
  void set_sync_off(double off);

  // ---- layout (one function per tab; each fills the content rect it is given) ---
  void layout(float w, float h);
  void relayout();
  // The stacked column, positioned for a given scroll offset.
  void layout_stacked(const ui::RectF& c, float scroll);
  // One builder per group: the column and the tabs both use these, so the two
  // arrangements cannot drift apart. Each returns the height the group needs at this
  // width, which is what stops a group drawing outside its own card.
  float build_videos(float x, float y, float w);
  float build_playback(float x, float y, float w);
  float build_timelines(float x, float y, float w, float min_h);
  float build_volume(float x, float y, float w);
  float build_windows(float x, float y, float w, bool with_shortcuts);
  float build_settings_groups(float x, float y, float w, bool with_about);
  void layout_settings_page(const ui::RectF& c);
  void layout_sources(const ui::RectF& c);
  void layout_sync(const ui::RectF& c);
  void layout_windows(const ui::RectF& c);
  void layout_settings(const ui::RectF& c);

  // ---- drawing -------------------------------------------------------------
  void draw_title_bar();
  void draw_tabs();
  void draw_stacked();
  void draw_settings_page();
  void draw_scrollbar();
  void draw_sources_tab();
  void draw_sync_tab();
  void draw_windows_tab();
  void draw_settings_tab();
  void draw_status();

  HWND hwnd_ = nullptr;
  ui::Renderer renderer_;
  ui::Controls ui_;
  ui::InputState in_;
  Tab tab_ = Tab::Sources;

  // chrome
  ui::RectF title_bar_, btn_min_, btn_close_, btn_settings_, tab_strip_, content_,
      status_strip_;
  ui::RectF tab_rect_[kTabCount];

  // sources
  ui::RectF card_videos_, btn_start_, src_hint_;
  SourceRow src_row_[2];

  // sync
  ui::RectF card_playback_, card_timelines_, card_sync_;
  ui::RectF btn_max_;
  ui::RectF btn_sync_play_, btn_back_, btn_fwd_, field_jump_, field_speed_, toggle_lock_;
  TimelineRow tl_[3];
  ui::RectF field_goto_, field_offset_, lbl_offset_;

  // windows
  ui::RectF card_windows_, card_volume_, card_shortcuts_;
  ui::RectF btn_arrange_, btn_pip_;
  ui::RectF vol_slider_[3], vol_label_[3];

  // settings
  ui::RectF card_appearance_, card_status_, card_tabs_, card_about_;
  ui::RectF toggle_dark_, toggle_readout_, toggle_tabs_;

  // the scrolled column
  ui::RectF scroll_track_, scroll_thumb_;
  float scroll_ = 0.0f, scroll_max_ = 0.0f, content_h_ = 0.0f;
  bool scroll_dragging_ = false;
  float scroll_drag_grab_ = 0.0f;
  bool tabs_mode_ = false;
  bool settings_page_ = false;  // the column's settings page is showing

  Config cfg_;
  MpvProcess movie_;
  MpvProcess reaction_;
  std::optional<void*> hwnd_movie_;
  std::optional<void*> hwnd_reaction_;
  bool started_ = false;
  bool playing_ = false;
  bool locked_ = false;
  bool floating_pip_ = false;
  bool dark_ = true;

  double sync_off_ = 0.0;
  double applied_rate_ = 1.0;
  double trim_since_ = 0.0;
  double vol_a_ = 100.0, vol_b_ = 100.0, vol_m_ = 100.0;
  double last_scrub_pos_ = 0.0;
  bool scrubbing_ = false;

  std::wstring src_a_text_, src_b_text_;
  std::wstring jump_text_, speed_text_, offset_text_, goto_text_;
  std::wstring status_text_, message_text_;
  double message_until_ = 0.0;

  // Cached label strings: rebuilt only when the value behind them changes, so a
  // playing frame draws from these instead of allocating fresh fmt/widen results.
  double cached_off_ = -1e300;
  std::wstring cached_jump_text_;
  std::wstring offset_label_cache_;
  std::wstring back_label_cache_, fwd_label_cache_;
  std::wstring vol_label_cache_[3];
  double cached_vol_[3] = {-1e300, -1e300, -1e300};
  double cached_time_a_[6] = {-1e300, -1e300, -1e300, -1e300, -1e300, -1e300};
  std::wstring time_a_cache_[3];
  // the About card shows the mpv path and the config dir, which do not change while
  // the panel is alive, so they are widened once and reused by every frame
  std::wstring about_mpv_cache_;
  bool about_mpv_valid_ = false;
  std::wstring about_cfg_cache_;
  std::wstring empty_text_ = L"";
  std::wstring chip_label_ = L"2.0.0";
  float chip_w_ = 0.0f;

  float width_ = 880.0f, height_ = 780.0f;
  float dpi_ = 96.0f;
  float scale_ = 1.0f;  // design units -> pixels
  // The divisor the last layout pass used. The mouse is converted with this, never with a
  // freshly computed one, so the pointer and the rects can never disagree.
  float input_scale_ = 1.0f;
  float scale_dpi_ = 1.0f;  // logical -> physical pixels for this display

  // repaint bookkeeping
  bool dirty_ = true;
  int applied_timer_ms_ = 0;
  double last_paint_pos_ = -1.0;
  bool last_playing_ = false;
  bool last_message_shown_ = false;
};

}  // namespace sp::app
