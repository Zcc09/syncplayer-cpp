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

  static constexpr float kMinWidth = 800.0f;
  static constexpr float kMinHeight = 700.0f;

  bool init(HWND hwnd);
  void shutdown();

  // called from the window procedure
  void on_input(const ui::InputState& in) { in_ = in; }
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
  };

  // ---- actions -------------------------------------------------------------
  void seek_side(Side side, double seconds);
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
  std::wstring offset_label() const;

  // ---- layout (one function per tab; each fills the content rect it is given) ---
  void layout(float w, float h);
  void relayout();
  // The stacked column, positioned for a given scroll offset.
  void layout_stacked(const ui::RectF& c, float scroll);
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
  ui::RectF card_videos_, card_actions_, btn_start_, btn_play_, src_hint_;
  SourceRow src_row_[2];

  // sync
  ui::RectF card_playback_, card_timelines_, card_align_;
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

  float width_ = 880.0f, height_ = 780.0f;
  float dpi_ = 96.0f;
  float scale_ = 1.0f;  // design units -> pixels
  float scale_dpi_ = 1.0f;  // logical -> physical pixels for this display

  // repaint bookkeeping
  bool dirty_ = true;
  int applied_timer_ms_ = 0;
  double last_paint_pos_ = -1.0;
  bool last_playing_ = false;
  bool last_message_shown_ = false;
};

}  // namespace sp::app
