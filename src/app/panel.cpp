#include "app/debug.h"
#include "app/panel.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shlobj.h>

#include "core/paths.h"

namespace sp::app {
namespace {

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  const int need = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                       nullptr, 0);
  std::wstring out(static_cast<size_t>(need), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), need);
  return out;
}

std::string narrow(const std::wstring& s) {
  if (s.empty()) return {};
  const int need = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<size_t>(need), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), need,
                      nullptr, nullptr);
  return out;
}

std::wstring fmt(const wchar_t* pattern, ...) {
  wchar_t buf[512];
  va_list args;
  va_start(args, pattern);
  _vsnwprintf_s(buf, _TRUNCATE, pattern, args);
  va_end(args);
  return buf;
}

double now_seconds() { return static_cast<double>(GetTickCount64()) / 1000.0; }


// Fluent metrics, shared by every tab
constexpr float kTitleBarH = 40.0f;
constexpr float kTabStripH = 46.0f;
constexpr float kCardTitleH = 18.0f;
constexpr float kCardTop = 16.0f;
constexpr float kLabelH = 16.0f;
constexpr float kFieldH = 36.0f;
constexpr float kRowGap = 4.0f;    // a label to its field
constexpr float kBlockGap = 8.0f;  // one labelled row to the next
constexpr float kStripH = 44.0f;
// A labelled field is a caption with the input under it, the way WinUI does it.
constexpr float kFieldRowH = kLabelH + kRowGap + kFieldH;

enum Id {
  ID_TAB0 = 10, ID_TAB1, ID_TAB2, ID_TAB3,
  ID_SRC_A = 100, ID_SRC_A_BROWSE, ID_SRC_B, ID_SRC_B_BROWSE,
  ID_START,
  ID_SYNC_PLAY, ID_BACK, ID_FWD, ID_JUMP, ID_SPEED, ID_LOCK,
  ID_BAR_MOVIE, ID_BAR_REACTION, ID_BAR_MASTER, ID_GOTO, ID_OFFSET,
  ID_PLAY_MOVIE, ID_PLAY_REACTION,
  ID_ARRANGE, ID_PIP,
  ID_VOL_A, ID_VOL_B, ID_VOL_M,
  ID_DARK, ID_READOUT, ID_TABS,
  ID_SETTINGS, ID_MIN, ID_CLOSE,
};

const wchar_t* const kTabNames[kTabCount] = {L"Sources", L"Sync", L"Windows",
                                             L"Settings"};

// Places blocks down a column, either at a fixed height or filling what is left, so a
// tab always occupies exactly the window it was given.

}  // namespace

// ---------------------------------------------------------------------------
bool Panel::init(HWND hwnd) {
  hwnd_ = hwnd;
  cfg_ = Config::load();
  dark_ = (cfg_.theme == "dark") || (cfg_.theme != "light" && ui::system_prefers_dark());
  // for_system(), not dark_theme()/light_theme() directly: it also applies the
  // system accent, which is what fills switches, sliders and accent buttons.
  renderer_.set_theme(ui::Theme::for_system(dark_));
  if (!renderer_.init(hwnd)) {
    MessageBoxW(hwnd, L"Direct2D could not be initialised.", L"SyncPlayer",
                MB_OK | MB_ICONERROR);
    return false;
  }
  ensure_dirs();
  src_a_text_ = widen(cfg_.movie);
  src_b_text_ = widen(cfg_.reaction);
  jump_text_ = fmt(L"%.1f", cfg_.jump_sec);
  speed_text_ = fmt(L"%.2f", cfg_.speed);
  offset_text_ = L"0.00";
  vol_a_ = cfg_.vol_a;
  vol_b_ = cfg_.vol_b;
  vol_m_ = cfg_.vol_m;
  message_text_ =
      L"Choose your movie and the reaction on the Sources tab, then press Start.";
  message_until_ = now_seconds() + 3600.0;
  set_dark(dark_);
  // reopen on the tab the user left, which is what a WinUI page does
  tab_ = static_cast<Tab>(std::clamp(cfg_.tab, 0, kTabCount - 1));
  tabs_mode_ = cfg_.ui_tabs;
  return true;
}

void Panel::shutdown() {
  stop_players();
  cfg_.movie = narrow(src_a_text_);
  cfg_.reaction = narrow(src_b_text_);
  cfg_.vol_a = vol_a_;
  cfg_.vol_b = vol_b_;
  cfg_.vol_m = vol_m_;
  cfg_.jump_sec = jump_seconds();
  cfg_.theme = dark_ ? "dark" : "light";
  cfg_.tab = static_cast<int>(tab_);
  cfg_.ui_tabs = tabs_mode_;
  cfg_.save();
  renderer_.shutdown();
}

void Panel::remember_window(int x, int y, int w, int h) {
  // Stored in logical pixels: the Python build writes and reads the same file and is
  // DPI-unaware, so a physical size would come back as a window 150% too large on a
  // 150% display.
  const float s = (scale_dpi_ > 0.0f) ? scale_dpi_ : 1.0f;
  cfg_.window = WindowRect{x, y, static_cast<int>(w / s), static_cast<int>(h / s)};
}

bool Panel::editing_text() const {
  const unsigned f = ui_.focused();
  return f == ID_SRC_A || f == ID_SRC_B || f == ID_JUMP || f == ID_SPEED ||
         f == ID_GOTO || f == ID_OFFSET;
}

void Panel::on_wheel(int delta) {
  if (tabs_mode_ || scroll_max_ <= 0.0f) return;
  scroll_ = std::clamp(scroll_ - (delta / 120.0f) * 64.0f, 0.0f, scroll_max_);
  relayout();
  dirty_ = true;
}

int Panel::min_height() const {
  // A stacked column scrolls, so it needs far less room than a tabbed one, which has to
  // show a whole tab.
  return static_cast<int>(tabs_mode_ ? kMinHeight : 520.0f);
}

void Panel::set_dark(bool dark) {
  dark_ = dark;
  renderer_.set_theme(ui::Theme::for_system(dark_));
  const BOOL use_dark = dark_ ? TRUE : FALSE;
  constexpr DWORD kImmersiveDarkMode = 20;
  DwmSetWindowAttribute(hwnd_, kImmersiveDarkMode, &use_dark, sizeof use_dark);
  cfg_.theme = dark_ ? "dark" : "light";
}

void Panel::on_resize(float w, float h) {
  width_ = w;
  height_ = h;

  // The window's DPI, so the render target maps one design unit to dpi/96 pixels and the
  // interface keeps its designed proportions on a scaled display, instead of drawing a 14
  // unit caption as 14 physical pixels.
  const UINT window_dpi = GetDpiForWindow(hwnd_);
  dpi_ = window_dpi ? static_cast<float>(window_dpi) : 96.0f;
  scale_dpi_ = dpi_ / 96.0f;

  renderer_.resize(static_cast<UINT>(w), static_cast<UINT>(h), dpi_);

  // Design units are device-independent pixels: the client size divided by the display
  // scale. The fit below is only a safety net for a window smaller than the minimum, and
  // the minimum is measured in the same units.
  const float dip_w = w / scale_dpi_;
  const float dip_h = h / scale_dpi_;
  scale_ = std::min(1.0f, std::min(dip_w / kMinWidth, dip_h / kMinHeight));
  renderer_.set_scale(scale_);
  layout(dip_w / scale_, dip_h / scale_);
}

void Panel::relayout() {
  layout(width_ / (scale_dpi_ * scale_), height_ / (scale_dpi_ * scale_));
}

void Panel::on_dpi(float dpi) {
  dpi_ = (dpi > 0.0f) ? dpi : 96.0f;
  scale_dpi_ = dpi_ / 96.0f;
  renderer_.resize(static_cast<UINT>(width_), static_cast<UINT>(height_), dpi_);
  const float dip_w = width_ / scale_dpi_;
  const float dip_h = height_ / scale_dpi_;
  scale_ = std::min(1.0f, std::min(dip_w / kMinWidth, dip_h / kMinHeight));
  renderer_.set_scale(scale_);
  layout(dip_w / scale_, dip_h / scale_);
  dirty_ = true;
}

void Panel::select_tab(int index) {
  if (index < 0 || index >= kTabCount) return;
  tab_ = static_cast<Tab>(index);
  relayout();
  dirty_ = true;  // the new tab lays itself out
}

void Panel::set_message(const std::wstring& text) {
  message_text_ = text;
  message_until_ = now_seconds() + 8.0;
  dirty_ = true;
}

double Panel::jump_seconds() const {
  try {
    return std::clamp(std::stod(jump_text_), 0.5, 120.0);
  } catch (...) {
    return 5.0;
  }
}

std::optional<double> Panel::parse_timecode(const std::wstring& text) const {
  std::wstring cleaned;
  for (wchar_t c : text) {
    if (c == L',') cleaned.push_back(L'.');
    else if (c != L' ') cleaned.push_back(c);
  }
  if (cleaned.empty()) return std::nullopt;
  double sign = 1.0;
  if (cleaned.front() == L'-') {
    sign = -1.0;
    cleaned.erase(cleaned.begin());
  } else if (cleaned.front() == L'+') {
    cleaned.erase(cleaned.begin());
  }
  if (cleaned.empty()) return std::nullopt;  // a lone sign is not a value
  try {
    // hh:mm:ss, mm:ss or plain seconds
    std::vector<std::wstring> parts;
    std::wstring part;
    for (wchar_t c : cleaned) {
      if (c == L':') {
        parts.push_back(part);
        part.clear();
      } else {
        part.push_back(c);
      }
    }
    parts.push_back(part);
    double total = 0.0;
    for (const auto& p : parts) total = total * 60.0 + std::stod(p);
    return sign * total;
  } catch (...) {
    return std::nullopt;
  }
}

const std::wstring& Panel::offset_label() {
  if (std::fabs(sync_off_ - cached_off_) > 1e-9) {
    cached_off_ = sync_off_;
    if (std::fabs(sync_off_) < 0.005)
      offset_label_cache_ = L"Videos are aligned";
    else
      offset_label_cache_ = fmt(L"Reaction runs %+.2fs against the movie", sync_off_);
  }
  return offset_label_cache_;
}

const std::wstring& Panel::jump_back_label() {
  // the label is a pure function of jump_text_, so while playing (where the field
  // does not change) neither the stod parse nor the fmt runs again
  if (jump_text_ != cached_jump_text_) {
    cached_jump_text_ = jump_text_;
    const double jump = jump_seconds();
    back_label_cache_ = fmt(L"\u2190 %gs", jump);
    fwd_label_cache_ = fmt(L"%gs \u2192", jump);
  }
  return back_label_cache_;
}

const std::wstring& Panel::jump_fwd_label() {
  jump_back_label();
  return fwd_label_cache_;
}

const std::wstring& Panel::time_label(unsigned row, double pos, double dur) {
  // format_hms prints whole seconds, so the string only changes when the whole
  // second changes; while playing the position moves every frame, and keying on
  // the raw value would rebuild the string (and two widen() calls) every frame
  const long lp = static_cast<long>(pos > 0.0 ? pos : 0.0);
  const long ld = static_cast<long>(dur > 0.0 ? dur : 0.0);
  if (lp != cached_time_a_[row * 2] || ld != cached_time_a_[row * 2 + 1]) {
    cached_time_a_[row * 2] = lp;
    cached_time_a_[row * 2 + 1] = ld;
    time_a_cache_[row] = fmt(L"%s / %s", widen(format_hms(pos)).c_str(),
                             widen(format_hms(dur)).c_str());
  }
  return time_a_cache_[row];
}

const std::wstring& Panel::vol_label(unsigned i) {
  const wchar_t* names[3] = {L"Movie", L"Reaction", L"Master"};
  const double* values[3] = {&vol_a_, &vol_b_, &vol_m_};
  const double v = *values[i];
  if (v != cached_vol_[i]) {
    cached_vol_[i] = v;
    vol_label_cache_[i] = fmt(L"%s  %.0f%%", names[i], v);
  }
  return vol_label_cache_[i];
}

void Panel::set_sync_off(double off) {
  sync_off_ = off;
}

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------
void Panel::layout(float w, float h) {
  // Remember the divisor used to place the rects, so the mouse is converted with the same one.
  input_scale_ = scale_dpi_ * scale_;
  title_bar_ = {0, 0, w, kTitleBarH};
  btn_close_ = {w - 46.0f, 0, 46.0f, kTitleBarH};
  btn_min_ = {w - 92.0f, 0, 46.0f, kTitleBarH};
  tab_strip_ = {0, kTitleBarH, w, kTabStripH};

  // the status strip is pinned to the bottom in both arrangements
  status_strip_ = {ui::kPad, h - kStripH - 8.0f, w - 2 * ui::kPad, kStripH};

  if (tabs_mode_) {
    tab_strip_ = {0, kTitleBarH, w, kTabStripH};
    const float content_top = kTitleBarH + kTabStripH + 12.0f;
    content_ = {ui::kPad, content_top, w - 2 * ui::kPad,
                std::max(160.0f, status_strip_.y - content_top - 12.0f)};

    // tab widths follow their labels, so a longer name never clips
    float tx = ui::kPad;
    for (int i = 0; i < kTabCount; ++i) {
      const float tw = renderer_.measure(kTabNames[i], 14.0f) + 32.0f;
      tab_rect_[i] = {tx, kTitleBarH + 7.0f, tw, 32.0f};
      tx += tw + 6.0f;
    }

    switch (tab_) {
      case Tab::Sources: layout_sources(content_); break;
      case Tab::Sync: layout_sync(content_); break;
      case Tab::Windows: layout_windows(content_); break;
      case Tab::Settings: layout_settings(content_); break;
      default: break;
    }
    return;
  }

  // the stacked column: no tab strip, so the content starts right under the title bar
  tab_strip_ = {};
  const float content_top = kTitleBarH + 12.0f;
  content_ = {ui::kPad, content_top, w - 2 * ui::kPad,
              std::max(120.0f, status_strip_.y - content_top - 12.0f)};

  if (settings_page_) {
    layout_settings_page(content_);
    scroll_max_ = 0.0f;
    scroll_ = 0.0f;
    return;
  }

  // Measure first, then place: the height of the column decides how far it can scroll.
  layout_stacked(content_, 0.0f);
  scroll_max_ = std::max(0.0f, content_h_ - content_.h);
  scroll_ = std::clamp(scroll_, 0.0f, scroll_max_);
  layout_stacked(content_, scroll_);
}

// ---------------------------------------------------------------------------
// the stacked column: one group under another, the way the Python build reads
// ---------------------------------------------------------------------------
namespace {

// Places items left to right inside a group and moves one to the next line when it does not
// fit, then reports the height used so the group can size itself to its contents. This is what
// keeps elements inside their own group at any window width instead of over the next one.
struct Flow {
  float x0 = 0.0f, y0 = 0.0f, max_w = 0.0f, gap = 8.0f;
  float x = 0.0f, y = 0.0f, line_h = 0.0f, bottom = 0.0f;

  void begin(float x_, float y_, float w, float g) {
    x0 = x_; y0 = y_; max_w = w; gap = g;
    x = x0; y = y0; line_h = 0.0f; bottom = y0;
  }

  ui::RectF place(float w, float h) {
    if (w > max_w) w = std::max(max_w, 1.0f);
    if (x > x0 && x + w > x0 + max_w) {  // no room on this line
      y += line_h + gap;
      x = x0;
      line_h = 0.0f;
    }
    const ui::RectF r{x, y, w, h};
    x += w + gap;
    if (h > line_h) line_h = h;
    if (y + h > bottom) bottom = y + h;
    return r;
  }

  float height() const { return bottom - y0; }
};

}  // namespace

float Panel::build_videos(float x, float y, float w) {
  const float inner = std::max(80.0f, w - 2 * ui::kPad);
  Flow f;
  f.begin(x + ui::kPad, y + kCardTop + kCardTitleH + 8.0f, inner, ui::kGap);

  const float field_w = std::max(150.0f, std::min(420.0f, (inner - ui::kGap) * 0.5f));
  const float field_h = kLabelH + kRowGap + kFieldH;
  // The button is measured against its own caption (drawn at 14pt, centred, with room
  // for hover), so "Browse…" can never be clipped by its box.
  const float browse_w = std::max(48.0f, renderer_.measure(L"Browse\u2026", 14.0f) + 24.0f);
  SourceRow* fields[2] = {&src_row_[0], &src_row_[1]};
  for (SourceRow* fld : fields) {
    const ui::RectF it = f.place(field_w, field_h);
    fld->label = {it.x, it.y, it.w, kLabelH};
    fld->browse = {it.x + it.w - browse_w, it.y + kLabelH + kRowGap, browse_w, kFieldH};
    fld->field = {it.x, it.y + kLabelH + kRowGap, std::max(60.0f, it.w - browse_w - ui::kGap), kFieldH};
  }

  src_hint_ = f.place(inner, 40.0f);
  card_videos_ = {x, y, w, kCardTop + kCardTitleH + 8.0f + f.height() + 12.0f};
  return card_videos_.h;
}

float Panel::build_playback(float x, float y, float w) {
  const float inner = std::max(80.0f, w - 2 * ui::kPad);
  Flow f;
  f.begin(x + ui::kPad, y + kCardTop + kCardTitleH + 8.0f, inner, ui::kGap);

  // Every item on this line is as tall as a labelled field, with its control at the bottom
  // of that height, so the buttons line up with the input boxes rather than sitting above
  // them. The label row of a button is simply empty.
  const float labelled = kLabelH + kRowGap + kFieldH;
  auto place_control = [&](float w) {
    const ui::RectF it = f.place(w, labelled);
    return ui::RectF{it.x, it.y + kLabelH + kRowGap, it.w, kFieldH};
  };
  btn_start_ = place_control(150.0f);
  btn_sync_play_ = place_control(150.0f);
  btn_back_ = place_control(96.0f);
  btn_fwd_ = place_control(96.0f);

  const ui::RectF jit = f.place(120.0f, labelled);
  field_jump_ = {jit.x, jit.y + kLabelH + kRowGap, jit.w, kFieldH};
  const ui::RectF sit = f.place(120.0f, labelled);
  field_speed_ = {sit.x, sit.y + kLabelH + kRowGap, sit.w, kFieldH};

  const ui::RectF lit = f.place(std::min(260.0f, inner), labelled);
  toggle_lock_ = {lit.x, lit.y + kLabelH + kRowGap, lit.w, kFieldH};

  card_playback_ = {x, y, w, kCardTop + kCardTitleH + 8.0f + f.height() + 12.0f};
  return card_playback_.h;
}

float Panel::build_timelines(float x, float y, float w, float min_h) {
  const float inner = std::max(80.0f, w - 2 * ui::kPad);
  const float lw = std::min(58.0f, inner * 0.3f);
  const float tw = 92.0f;
  const float pb = 26.0f;   // the per-video play/pause button takes this off the bar
  const float goto_w = 170.0f;  // the Go to box, which lives on the Master row
  float cy = y + kCardTop + kCardTitleH + 8.0f;

  for (int i = 0; i < 3; ++i) {
    const bool master = (i == 2);
    // The Master row is taller: its Go to box carries a label, so the row's own bar, button
    // and readout are placed on the same line as that box's input.
    const float row_h = master ? (kLabelH + kRowGap + kFieldH + 8.0f) : 38.0f;
    const float ctrl_y = master ? cy + kLabelH + kRowGap + (kFieldH - 20.0f) / 2.0f
                                : cy + 2.0f;
    const float beside = inner - lw - tw - ui::kGap;
    const float taken = (i < 2 ? pb + ui::kGap : 0.0f) + (master ? goto_w + ui::kGap : 0.0f);
    const float bar_avail = beside - taken;

    if (bar_avail >= 110.0f) {
      tl_[i].label = {x + ui::kPad, cy, lw, kLabelH};
      tl_[i].bar = {x + ui::kPad + lw, ctrl_y, bar_avail, 20.0f};
      if (i < 2) {
        tl_[i].play = {x + ui::kPad + lw + bar_avail + ui::kGap, ctrl_y - 2.0f, pb, 24.0f};
      }
      tl_[i].time = {x + ui::kPad + lw + beside + ui::kGap, ctrl_y, tw, kLabelH + 2.0f};
      if (master) {
        // next to the main timeline, sharing its row
        field_goto_ = {x + ui::kPad + lw + beside + ui::kGap + tw + ui::kGap,
                       cy + kLabelH + kRowGap, goto_w, kFieldH};
      }
      cy += row_h;
    } else {
      // no room beside the bar: the readout goes underneath
      const float sub = x + ui::kPad + lw;
      const float bar_w = std::max(60.0f, inner - lw - (i < 2 ? pb + ui::kGap : 0.0f));
      tl_[i].label = {x + ui::kPad, cy, lw, kLabelH};
      tl_[i].bar = {sub, ctrl_y, bar_w, 20.0f};
      if (i < 2) {
        tl_[i].play = {sub + bar_w + ui::kGap, ctrl_y - 2.0f, pb, 24.0f};
      }
      tl_[i].time = {sub, ctrl_y + 26.0f, bar_w, kLabelH};
      if (master) {
        field_goto_ = {sub, tl_[i].time.y + kLabelH + kRowGap, std::min(goto_w, inner), kFieldH};
      }
      cy += row_h + 12.0f;
    }
  }
  cy += 6.0f;

  const float needed = cy - y;
  card_timelines_ = {x, y, w, std::max(needed, min_h)};
  return card_timelines_.h;
}

float Panel::build_volume(float x, float y, float w) {
  const float inner = std::max(80.0f, w - 2 * ui::kPad);
  const float lw = std::min(96.0f, inner * 0.4f);
  float cy = y + kCardTop + kCardTitleH + 8.0f;

  for (int i = 0; i < 3; ++i) {
    const float sw = inner - lw - ui::kGap;
    if (sw >= 90.0f) {
      vol_label_[i] = {x + ui::kPad, cy, lw, kLabelH};
      vol_slider_[i] = {x + ui::kPad + lw, cy + 4.0f, sw, 20.0f};
      cy += 34.0f;
    } else {
      vol_label_[i] = {x + ui::kPad, cy, inner, kLabelH};
      vol_slider_[i] = {x + ui::kPad, cy + kLabelH, std::max(60.0f, inner), 20.0f};
      cy += 40.0f;
    }
  }

  card_volume_ = {x, y, w, cy - y + 4.0f};
  return card_volume_.h;
}

float Panel::build_windows(float x, float y, float w, bool with_shortcuts) {
  const float inner = std::max(80.0f, w - 2 * ui::kPad);
  Flow f;
  f.begin(x + ui::kPad, y + kCardTop + kCardTitleH + 8.0f, inner, ui::kGap);
  btn_arrange_ = f.place(std::min(220.0f, inner), kFieldH);
  btn_pip_ = f.place(std::min(200.0f, inner), kFieldH);

  // the hint is anchored to the bottom of the card by the draw, so reserve its room here
  card_windows_ = {x, y, w, kCardTop + kCardTitleH + 8.0f + f.height() + 34.0f + 12.0f};
  float used = card_windows_.h + ui::kGap;

  if (with_shortcuts) {
    const float rows = 4 * 22.0f;
    card_shortcuts_ = {x, y + used, w, kCardTop + kCardTitleH + 8.0f + rows + 12.0f};
    used += card_shortcuts_.h;
  } else {
    card_shortcuts_ = {};
  }
  return used - ui::kGap;
}

float Panel::build_settings_groups(float x, float y, float w, bool with_about) {
  const float inner = std::max(80.0f, w - 2 * ui::kPad);
  const float row = 34.0f;
  float cy = y;

  ui::RectF* toggles[3] = {&toggle_dark_, &toggle_readout_, &toggle_tabs_};
  ui::RectF* cards[3] = {&card_appearance_, &card_status_, &card_tabs_};
  for (int i = 0; i < 3; ++i) {
    *cards[i] = {x, cy, w, kCardTop + kCardTitleH + 8.0f + row + 12.0f};
    *toggles[i] = {x + ui::kPad, cy + kCardTop + kCardTitleH + 8.0f, inner, row};
    cy += cards[i]->h + ui::kGap;
  }

  // sync: the typed offset and what it currently is, moved out of the timelines group
  card_sync_ = {x, cy, w, kCardTop + kCardTitleH + 8.0f + kLabelH + kRowGap + kFieldH +
                            6.0f + kLabelH + 12.0f};
  field_offset_ = {x + ui::kPad, card_sync_.y + kCardTop + kCardTitleH + 8.0f + kLabelH +
                                  kRowGap, std::min(200.0f, inner), kFieldH};
  lbl_offset_ = {x + ui::kPad, field_offset_.y + kFieldH + 6.0f, inner, kLabelH};
  cy += card_sync_.h + ui::kGap;

  if (with_about) {
    card_about_ = {x, cy, w, kCardTop + kCardTitleH + 8.0f + 3 * 24.0f + 12.0f};
    cy += card_about_.h;
  } else {
    card_about_ = {};
  }
  return cy - y;
}

void Panel::layout_stacked(const ui::RectF& c, float scroll) {
  float y = c.y - scroll;
  y += build_videos(c.x, y, c.w) + ui::kGap;
  y += build_playback(c.x, y, c.w) + ui::kGap;
  y += build_timelines(c.x, y, c.w, 0.0f) + ui::kGap;
  y += build_volume(c.x, y, c.w) + ui::kGap;
  y += build_windows(c.x, y, c.w, false);
  // appearance, the status bar, the arrangement and about live on the settings page
  card_appearance_ = card_status_ = card_tabs_ = card_about_ = card_sync_ = {};
  content_h_ = y + scroll - c.y;
}

void Panel::layout_sources(const ui::RectF& c) {
  // The buttons that load and start the videos belong to the playback group, so this tab is
  // the two video fields and the hint, and the card sizes itself to them.
  build_videos(c.x, c.y, c.w);
  card_playback_ = card_timelines_ = card_volume_ = card_windows_ = card_shortcuts_ = {};
}

void Panel::layout_sync(const ui::RectF& c) {
  const float ph = build_playback(c.x, c.y, c.w);
  const float ty = c.y + ph + ui::kGap;
  // the timelines card fills what is left, but never less than its contents need
  build_timelines(c.x, ty, c.w, std::max(0.0f, c.h - ph - ui::kGap));
  card_videos_ = card_volume_ = card_windows_ = card_shortcuts_ = card_sync_ = {};
}

void Panel::layout_windows(const ui::RectF& c) {
  const float wh = build_windows(c.x, c.y, c.w, true);
  const float vy = c.y + wh + ui::kGap;
  // the volume card takes the rest of the tab
  const float vh = build_volume(c.x, vy, c.w);
  card_volume_.h = std::max(vh, c.h - wh - ui::kGap);
  card_videos_ = card_playback_ = card_timelines_ = {};
}

void Panel::layout_settings(const ui::RectF& c) {
  const float h = build_settings_groups(c.x, c.y, c.w, true);
  card_about_.h = std::max(card_about_.h, c.h - (h - card_about_.h));
  card_videos_ = card_playback_ = card_timelines_ = card_volume_ = card_windows_ = {};
  card_shortcuts_ = {};
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
void Panel::draw() {
  // Trust the window over our own record of it: a late WM_DPICHANGED can resize the
  // window after the initial clamp, and a layout left at the old size would push the
  // last card off the bottom.
  {
    RECT rc{};
    if (GetClientRect(hwnd_, &rc)) {
      const float cw = static_cast<float>(rc.right - rc.left);
      const float ch = static_cast<float>(rc.bottom - rc.top);
      if (cw > 1.0f && ch > 1.0f &&
          (std::fabs(cw - width_) > 0.5f || std::fabs(ch - height_) > 0.5f)) {
        on_resize(cw, ch);
        dirty_ = true;
      }
    }
  }

  // input arrives in pixels; the layout works in design units
  ui::InputState mapped = in_;
  if (scale_ > 0.0f) {
    mapped.mouse_x = in_.mouse_x / (scale_dpi_ * scale_);
    mapped.mouse_y = in_.mouse_y / (scale_dpi_ * scale_);
  }
  ui_.begin_frame(renderer_, mapped);

  const ui::Theme& t = renderer_.theme();
  const float dw = design_w(), dh = design_h();
  renderer_.begin();
  renderer_.fill_gradient_v({0, 0, dw, dh},
                            dark_ ? ui::Color{0.11f, 0.11f, 0.12f, 1.0f}
                                  : ui::Color{0.97f, 0.97f, 0.98f, 1.0f},
                            t.window_bg);

  draw_title_bar();
  if (tabs_mode_) {
    draw_tabs();
    switch (tab_) {
      case Tab::Sources: draw_sources_tab(); break;
      case Tab::Sync: draw_sync_tab(); break;
      case Tab::Windows: draw_windows_tab(); break;
      case Tab::Settings: draw_settings_tab(); break;
      default: break;
    }
  } else if (settings_page_) {
    renderer_.clip_push(content_);
    ui_.set_active_area(content_);
    draw_settings_page();
    ui_.set_active_area(ui::RectF{});
    renderer_.clip_pop();
  } else {
    // The column scrolls, so it is drawn inside a clip and only the visible part takes
    // input: a control that has scrolled out of sight must not answer a click.
    renderer_.clip_push(content_);
    ui_.set_active_area(content_);
    draw_stacked();
    ui_.set_active_area(ui::RectF{});
    renderer_.clip_pop();
  }
  draw_status();
  if (!tabs_mode_ && scroll_max_ > 0.0f) draw_scrollbar();

  renderer_.end();
  ui_.end_frame();

  dirty_ = false;  // until something changes again
}

void Panel::draw_title_bar() {
  const ui::Theme& t = renderer_.theme();
  renderer_.fill_rect(title_bar_,
                      dark_ ? ui::Color{0, 0, 0, 0.18f} : ui::Color{0, 0, 0, 0.03f});
  renderer_.text(L"SyncPlayer", {ui::kPad, 0, 200, kTitleBarH}, t.text, 14.0f,
                 ui::TextAlign::Left, ui::TextWeight::SemiBold);
  // "2.0.0" never changes, so the measure is paid for once, not every frame
  if (chip_w_ == 0.0f) chip_w_ = renderer_.measure(chip_label_, 12.0f) + 20.0f;
  const ui::RectF chip_rect{ui::kPad + 96.0f, 12.0f, chip_w_, 16.0f};
  renderer_.fill_rounded(chip_rect, 8.0f, t.control);
  renderer_.text(chip_label_, chip_rect, t.text_secondary, 12.0f,
                 ui::TextAlign::Center);

  // A settings button, in the column arrangement only: with tabs on, the Settings tab is
  // already there.
  if (!tabs_mode_) {
    const float bw = 96.0f;
    btn_settings_ = {btn_min_.x - bw - 8.0f, 8.0f, bw, 24.0f};
    // Standard, not Subtle: a borderless button in the title bar is hard to find.
    if (ui_.button(ID_SETTINGS, btn_settings_,
                   settings_page_ ? L"Back" : L"Settings",
                   ui::ButtonStyle::Standard)) {
      settings_page_ = !settings_page_;
      scroll_ = 0.0f;
      layout(width_ / (scale_dpi_ * scale_), height_ / (scale_dpi_ * scale_));
      dirty_ = true;
    }
  }

  if (ui_.icon_button(ID_MIN, btn_min_, L"\u2500", t.text)) {
    ShowWindow(hwnd_, SW_MINIMIZE);
  }
  if (ui_.icon_button(ID_CLOSE, btn_close_, L"\u2715", t.text)) {
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
  }
}

void Panel::draw_tabs() {
  const ui::Theme& t = renderer_.theme();
  renderer_.line(tab_strip_.x, tab_strip_.y + tab_strip_.h - 1.0f,
                 tab_strip_.x + tab_strip_.w, tab_strip_.y + tab_strip_.h - 1.0f,
                 t.border, 1.0f);

  const unsigned ids[kTabCount] = {ID_TAB0, ID_TAB1, ID_TAB2, ID_TAB3};
  const float mx = in_.mouse_x / (scale_dpi_ * scale_);
  const float my = in_.mouse_y / (scale_dpi_ * scale_);
  for (int i = 0; i < kTabCount; ++i) {
    const bool selected = (static_cast<int>(tab_) == i);
    const bool hovered = tab_rect_[i].contains(mx, my);
    // the control gives us the click; the selected look is drawn on top of it
    if (ui_.button(ids[i], tab_rect_[i], kTabNames[i], ui::ButtonStyle::Subtle)) {
      tab_ = static_cast<Tab>(i);
      relayout();  // the new tab lays itself out
      message_until_ = 0.0;
    }
    if (selected) {
      renderer_.fill_rounded(tab_rect_[i], ui::kRadiusControl, t.control);
      renderer_.text(kTabNames[i], tab_rect_[i], t.text, 14.0f, ui::TextAlign::Center,
                     ui::TextWeight::SemiBold);
      renderer_.fill_rounded({tab_rect_[i].x + 8.0f, tab_strip_.y + tab_strip_.h - 3.0f,
                              tab_rect_[i].w - 16.0f, 2.0f},
                             1.0f, t.accent);
    } else {
      if (hovered) {
        renderer_.fill_rounded(tab_rect_[i], ui::kRadiusControl, t.control_hover);
      }
      renderer_.text(kTabNames[i], tab_rect_[i], hovered ? t.text : t.text_secondary,
                     14.0f, ui::TextAlign::Center);
    }
  }
}

void Panel::draw_stacked() {
  // The group functions are the same ones the tabs use; here they are stacked, and the
  // layout pass has already placed every card for the current scroll offset. The settings
  // groups are not here: they are on the page the title bar's button opens.
  draw_sources_tab();
  draw_sync_tab();
  draw_windows_tab();
}

void Panel::draw_settings_page() {
  const ui::Theme& t = renderer_.theme();
  // The four settings groups, laid out down the page by layout_settings_page().
  renderer_.text(L"Settings", {content_.x, content_.y - 34.0f, content_.w, 24.0f}, t.text,
                 18.0f, ui::TextAlign::Left, ui::TextWeight::SemiBold);
  draw_settings_tab();
}

void Panel::layout_settings_page(const ui::RectF& c) {
  build_settings_groups(c.x, c.y + 6.0f, c.w, true);
  card_videos_ = card_playback_ = card_timelines_ = card_volume_ = card_windows_ = {};
  card_shortcuts_ = {};
}

void Panel::draw_scrollbar() {
  const ui::Theme& t = renderer_.theme();
  const float track_w = 5.0f;
  scroll_track_ = {content_.x + content_.w + 4.0f, content_.y, track_w, content_.h};

  const float visible = content_.h;
  const float ratio = std::clamp(visible / std::max(content_h_, 1.0f), 0.08f, 1.0f);
  const float thumb_h = std::max(40.0f, visible * ratio);
  const float travel = std::max(1.0f, visible - thumb_h);
  const float frac = (scroll_max_ > 0.0f)
                         ? std::clamp(scroll_ / scroll_max_, 0.0f, 1.0f)
                         : 0.0f;
  scroll_thumb_ = {scroll_track_.x, content_.y + travel * frac, track_w, thumb_h};

  const float mx = in_.mouse_x / (scale_dpi_ * scale_);
  const float my = in_.mouse_y / (scale_dpi_ * scale_);
  const bool over_thumb = scroll_thumb_.contains(mx, my);
  const bool over_track = scroll_track_.contains(mx, my);
  if (in_.mouse_pressed && over_thumb) {
    scroll_dragging_ = true;
    scroll_drag_grab_ = my - scroll_thumb_.y;
  } else if (in_.mouse_pressed && over_track) {
    scroll_ = std::clamp((my - content_.y - thumb_h / 2.0f) / travel * scroll_max_, 0.0f,
                         scroll_max_);
    relayout();
    dirty_ = true;
  }
  if (scroll_dragging_) {
    if (in_.mouse_down) {
      const float want = (my - scroll_drag_grab_ - content_.y) / travel * scroll_max_;
      const float clamped = std::clamp(want, 0.0f, scroll_max_);
      if (std::fabs(clamped - scroll_) > 0.5f) {
        scroll_ = clamped;
        relayout();
        dirty_ = true;
      }
    } else {
      scroll_dragging_ = false;
    }
  }

  renderer_.fill_rounded(scroll_track_, track_w / 2.0f,
                         t.dark ? ui::Color{1, 1, 1, 0.06f} : ui::Color{0, 0, 0, 0.05f});
  const bool hot = over_thumb || scroll_dragging_;
  renderer_.fill_rounded(scroll_thumb_, track_w / 2.0f,
                         hot ? t.text_secondary : t.track_hover);
}

void Panel::draw_sources_tab() {
  const ui::Theme& t = renderer_.theme();
  ui_.card(card_videos_, L"VIDEOS");

  const wchar_t* names[2] = {L"Movie", L"Reaction"};
  const unsigned field_ids[2] = {ID_SRC_A, ID_SRC_B};
  const unsigned browse_ids[2] = {ID_SRC_A_BROWSE, ID_SRC_B_BROWSE};
  for (int i = 0; i < 2; ++i) {
    renderer_.text(names[i], src_row_[i].label, t.text_secondary, 12.0f);
    std::wstring& value = (i == 0) ? src_a_text_ : src_b_text_;
    const ui::FieldResult res =
        ui_.text_field(field_ids[i], src_row_[i].field, value,
                       i == 0 ? L"movie.mp4 or a link" : L"reaction.mp4 or a link");
    if (res != ui::FieldResult::None) {
      if (i == 0) cfg_.movie = narrow(src_a_text_);
      else cfg_.reaction = narrow(src_b_text_);
    }
    if (ui_.button(browse_ids[i], src_row_[i].browse, L"Browse\u2026")) {
      browse_for(i == 0 ? Side::Movie : Side::Reaction);
    }
  }
  renderer_.text(L"Paste a link, or drop a file onto the window.", src_hint_,
                 t.text_secondary, 12.0f);

}

void Panel::draw_sync_tab() {
  const ui::Theme& t = renderer_.theme();
  ui_.card(card_playback_, L"PLAYBACK");

  if (ui_.button(ID_START, btn_start_, started_ ? L"Reload both" : L"Start",
                 ui::ButtonStyle::Accent)) {
    start_sources();
  }
  if (ui_.button(ID_SYNC_PLAY, btn_sync_play_, playing_ ? L"Pause" : L"Play",
                 playing_ ? ui::ButtonStyle::Standard : ui::ButtonStyle::Accent,
                 started_)) {
    toggle_play();
  }
  if (ui_.button(ID_BACK, btn_back_, jump_back_label(),
                 ui::ButtonStyle::Standard, started_)) {
    nudge_jump(-1);
  }
  if (ui_.button(ID_FWD, btn_fwd_, jump_fwd_label(),
                 ui::ButtonStyle::Standard, started_)) {
    nudge_jump(+1);
  }
  renderer_.text(L"Jump (s)",
                 {field_jump_.x, field_jump_.y - kLabelH - kRowGap, field_jump_.w, kLabelH},
                 t.text_secondary, 12.0f);
  renderer_.text(L"Speed",
                 {field_speed_.x, field_speed_.y - kLabelH - kRowGap, field_speed_.w,
                  kLabelH},
                 t.text_secondary, 12.0f);
  if (ui_.text_field(ID_JUMP, field_jump_, jump_text_, L"5", true) ==
      ui::FieldResult::Submitted) {
    cfg_.jump_sec = jump_seconds();
  }
  if (ui_.text_field(ID_SPEED, field_speed_, speed_text_, L"1.00", true) ==
      ui::FieldResult::Submitted) {
    apply_speed_from_field();
  }
  if (ui_.toggle(ID_LOCK, toggle_lock_, locked_, L"Lock sync to the master bar")) {
    set_message(locked_ ? L"Sync locked: the Master bar drives both videos."
                        : L"Sync unlocked: align with the per-video bars.");
  }

  ui_.card(card_timelines_, L"TIMELINES");
  const std::optional<double> pos_a = movie_.position();
  const std::optional<double> dur_a = movie_.duration();
  const std::optional<double> pos_b = reaction_.position();
  const std::optional<double> dur_b = reaction_.duration();
  const bool have = started_;

  auto row = [&](int i, unsigned id, const wchar_t* name, const std::optional<double>& pos,
                 const std::optional<double>& dur, Side side) {
    renderer_.text(name, tl_[i].label, have ? t.text : t.text_disabled, 13.0f,
                   ui::TextAlign::Left, ui::TextWeight::SemiBold);
    const ui::SeekDrag drag = ui_.seek_bar(id, tl_[i].bar, pos.value_or(0),
                                           dur.value_or(0), have && dur.value_or(0) > 0);
    renderer_.text(time_label(i, pos.value_or(0), dur.value_or(0)),
                   tl_[i].time, t.text_secondary, 12.0f, ui::TextAlign::Right);
    scrubbing_ = drag.result == ui::SeekResult::Dragging;
    if (drag.result == ui::SeekResult::Clicked) {
      last_scrub_pos_ = drag.clicked_seconds;
      seek_side(side, drag.clicked_seconds);
    } else if (drag.result == ui::SeekResult::Dragging ||
               drag.result == ui::SeekResult::Released) {
      // precise scrubbing: sideways travel at a fixed gain, finer once lifted
      seek_side(side, scrub_target(last_scrub_pos_, drag.dx_px, drag.lift_px, dur));
    }
  };

  row(0, ID_BAR_MOVIE, L"Movie", pos_a, dur_a, Side::Movie);
  row(1, ID_BAR_REACTION, L"Reaction", pos_b, dur_b, Side::Reaction);

  // per-video play/pause: locked means the pair moves together, unlocked means
  // each button flips its own side only
  const bool playing_movie = locked_ ? playing_ : !movie_.paused();
  const bool playing_reaction = locked_ ? playing_ : !reaction_.paused();
  const ui::Color pb_tint = have ? t.text : t.text_disabled;
  const wchar_t* const pb_glyph[2] = {L"\u25B6", L"\u23F8"};  // play, pause
  if (ui_.icon_button(ID_PLAY_MOVIE, tl_[0].play, pb_glyph[playing_movie],
                      pb_tint)) {
    toggle_play_side(Side::Movie);
  }
  if (ui_.icon_button(ID_PLAY_REACTION, tl_[1].play, pb_glyph[playing_reaction],
                      pb_tint)) {
    toggle_play_side(Side::Reaction);
  }

  // the master bar drives both, keeping the offset
  renderer_.text(L"Master", tl_[2].label, have ? t.text : t.text_disabled, 13.0f,
                 ui::TextAlign::Left, ui::TextWeight::SemiBold);
  const ui::SeekDrag master = ui_.seek_bar(ID_BAR_MASTER, tl_[2].bar, pos_a.value_or(0),
                                           dur_a.value_or(0), have && locked_);
  renderer_.text(time_label(2, pos_a.value_or(0), dur_a.value_or(0)),
                 tl_[2].time, t.text_secondary, 12.0f, ui::TextAlign::Right);
  if (master.result == ui::SeekResult::Clicked) {
    last_scrub_pos_ = master.clicked_seconds;
    seek_master(master.clicked_seconds);
  } else if (master.result == ui::SeekResult::Dragging ||
             master.result == ui::SeekResult::Released) {
    seek_master(scrub_target(last_scrub_pos_, master.dx_px, master.lift_px, dur_a));
  }
  if (!locked_) {
    renderer_.text(L"master bar (lock sync to use it)", tl_[2].label, t.text_disabled,
                   12.0f);
  }

  renderer_.text(L"Go to",
                 {field_goto_.x, field_goto_.y - kLabelH - kRowGap, field_goto_.w, kLabelH},
                 t.text_secondary, 12.0f);
  if (ui_.text_field(ID_GOTO, field_goto_, goto_text_, L"90 or 1:30:00") ==
      ui::FieldResult::Submitted) {
    if (auto secs = parse_timecode(goto_text_)) {
      seek_master(*secs);
      set_message(fmt(L"Moved both videos to %s.", widen(format_hms(*secs)).c_str()));
    } else {
      set_message(L"That is not a time. Try 90, 1:30 or 1:30:00.");
    }
    goto_text_.clear();
  }

  // the offset belongs to the timelines group, so it is drawn above, in that card
}

void Panel::draw_windows_tab() {
  const ui::Theme& t = renderer_.theme();
  ui_.card(card_windows_, L"WINDOWS");
  if (ui_.button(ID_ARRANGE, btn_arrange_, L"Arrange side by side",
                 ui::ButtonStyle::Standard, started_)) {
    arrange_windows();
  }
  if (ui_.button(ID_PIP, btn_pip_, floating_pip_ ? L"Exit floating PiP" : L"Floating PiP",
                 ui::ButtonStyle::Standard, started_)) {
    toggle_floating_pip();
  }
  renderer_.text(L"Floating PiP makes the reaction borderless and always on top.",
                 {card_windows_.x + ui::kPad, card_windows_.y + card_windows_.h - 30.0f,
                  card_windows_.w - 2 * ui::kPad, 18.0f},
                 t.text_secondary, 12.0f);

  ui_.card(card_volume_, L"VOLUME");
  double* values[3] = {&vol_a_, &vol_b_, &vol_m_};
  const unsigned ids[3] = {ID_VOL_A, ID_VOL_B, ID_VOL_M};
  bool changed = false;
  for (int i = 0; i < 3; ++i) {
    renderer_.text(vol_label(i), vol_label_[i], t.text_secondary, 13.0f);
    changed |= ui_.slider(ids[i], vol_slider_[i], *values[i], 0, kVolumeMax);
  }
  if (changed) apply_volume();

  if (card_shortcuts_.h <= 0.0f) return;  // a tab convenience, not in the column
  ui_.card(card_shortcuts_, L"SHORTCUTS");
  float sy = card_shortcuts_.y + kCardTop + kCardTitleH + 8.0f;
  const wchar_t* keys[4][2] = {{L"Space", L"Play or pause both videos"},
                               {L"Left", L"Jump back by the jump amount"},
                               {L"Right", L"Jump forward by the jump amount"},
                               {L"Ctrl+1..4", L"Switch tabs (Ctrl+Tab cycles)"}};
  for (int i = 0; i < 4; ++i) {
    renderer_.text(keys[i][0], {card_shortcuts_.x + ui::kPad, sy, 80.0f, 18.0f}, t.text,
                   13.0f, ui::TextAlign::Left, ui::TextWeight::SemiBold);
    renderer_.text(keys[i][1],
                   {card_shortcuts_.x + ui::kPad + 88.0f, sy,
                    card_shortcuts_.w - 2 * ui::kPad - 88.0f, 18.0f},
                   t.text_secondary, 13.0f);
    sy += 20.0f;
  }
}

void Panel::draw_settings_tab() {
  const ui::Theme& t = renderer_.theme();
  ui_.card(card_appearance_, L"APPEARANCE");
  if (ui_.toggle(ID_DARK, toggle_dark_, dark_, L"Dark theme")) {
    set_dark(dark_);
    set_message(dark_ ? L"Dark theme." : L"Light theme.");
  }

  ui_.card(card_status_, L"STATUS BAR");
  if (ui_.toggle(ID_READOUT, toggle_readout_, cfg_.show_readout,
                 L"Show the live position readout")) {
    set_message(cfg_.show_readout ? L"Readout shown." : L"Readout hidden.");
  }

  if (card_tabs_.h > 0.0f) {
    ui_.card(card_tabs_, L"ARRANGEMENT");
    bool as_tabs = tabs_mode_;
    if (ui_.toggle(ID_TABS, toggle_tabs_, as_tabs, L"Split the groups into tabs")) {
      tabs_mode_ = as_tabs;
      cfg_.ui_tabs = tabs_mode_;
      settings_page_ = false;
      scroll_ = 0.0f;
      relayout();
      dirty_ = true;
      set_message(tabs_mode_ ? L"Groups split into tabs."
                             : L"One column, like the Python build.");
    }
  }

  if (card_sync_.h > 0.0f) {
    ui_.card(card_sync_, L"SYNC");
    renderer_.text(L"Offset",
                   {field_offset_.x, field_offset_.y - kLabelH - kRowGap, field_offset_.w,
                    kLabelH},
                   t.text_secondary, 12.0f);
    if (ui_.text_field(ID_OFFSET, field_offset_, offset_text_, L"12.5 / 1:05 / -3.25",
                       true) == ui::FieldResult::Submitted) {
      apply_offset_from_field();
    }
    if (lbl_offset_.h > 0.0f) {
      renderer_.text(offset_label(), lbl_offset_, t.text_secondary, 12.0f);
    }
  }

  if (card_about_.h <= 0.0f) return;
  ui_.card(card_about_, L"ABOUT");
  float ay = card_about_.y + kCardTop + kCardTitleH + 8.0f;
  // the About card is drawn by every playing frame in the column, but its values are
  // fixed for the life of the process, so probe for mpv and widen() only once
  static const std::wstring kVersionLine = L"2.0.0 (C++ build)";
  if (!about_mpv_valid_) {
    about_mpv_valid_ = true;
    const auto mpv = find_mpv();
    about_mpv_cache_ = mpv ? widen(to_utf8(*mpv)) : std::wstring(L"not found");
    about_cfg_cache_ = widen(to_utf8(config_dir() / "syncplayer_config.json"));
  }
  const struct {
    const wchar_t* label;
    const std::wstring& value;
  } lines[] = {
      {L"Version", kVersionLine},
      {L"mpv", about_mpv_cache_},
      {L"Config", about_cfg_cache_},
  };
  for (const auto& line : lines) {
    renderer_.text(line.label, {card_about_.x + ui::kPad, ay, 80.0f, 20.0f},
                   t.text_secondary, 13.0f);
    renderer_.text(line.value,
                   {card_about_.x + ui::kPad + 88.0f, ay,
                    card_about_.w - 2 * ui::kPad - 88.0f, 20.0f},
                   t.text, 13.0f);
    ay += 24.0f;
  }
  renderer_.text(L"This build reads the same settings file as the Python version, so "
                 L"your sources and remembered alignments carry over.",
                 {card_about_.x + ui::kPad, card_about_.y + card_about_.h - 52.0f,
                  card_about_.w - 2 * ui::kPad, 40.0f},
                 t.text_secondary, 12.0f);
}

void Panel::draw_status() {
  const ui::Theme& t = renderer_.theme();
  renderer_.fill_rounded(status_strip_, ui::kRadiusCard, t.layer);
  renderer_.stroke_rounded(status_strip_, ui::kRadiusCard, t.border, 1.0f);

  // drawn by reference: while playing status_text_ is non-empty every frame, so
  // the old local copy re-allocated the whole readout string on every frame
  static const std::wstring kReadoutIdle = L"Idle. Press Start on the Sources tab.";
  static const std::wstring kReadoutHidden = L"Readout hidden (Settings).";
  const std::wstring& readout =
      cfg_.show_readout ? (status_text_.empty() ? kReadoutIdle : status_text_)
                        : kReadoutHidden;
  renderer_.text(readout,
                 {status_strip_.x + 12.0f, status_strip_.y + 2.0f, status_strip_.w - 24.0f,
                  20.0f},
                 t.text, 13.0f);
  const bool show_message = now_seconds() < message_until_ && !message_text_.empty();
  // L"" used to build a throwaway string every frame; empty_text_ holds it once
  renderer_.text(show_message ? message_text_ : empty_text_,
                 {status_strip_.x + 12.0f, status_strip_.y + 21.0f, status_strip_.w - 24.0f,
                  18.0f},
                 t.text_secondary, 12.0f);
}

}  // namespace sp::app
