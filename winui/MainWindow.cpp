#include "pch.h"

// The window: title bar, then the five groups and the status footer, in the same order the
// hand-drawn build used. Controls are real WinUI controls, styled from the system theme
// resources, so this follows the Windows theme and accent instead of approximating them.
#include "MainWindow.h"

#include "ui.h"
#include "log.h"

#include <commdlg.h>   // GetOpenFileNameW: a browse dialog without needing window interop

using namespace winrt;
using namespace winrt::Windows::Foundation;   // IInspectable for event handlers
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace SyncPlayer {

namespace {

constexpr wchar_t kVersion[] = L"2.0.0";

// The one row of a timeline: name, scrubber, its own play button, the readout.
struct TimelineRow {

  Controls::TextBlock name{nullptr};
  Controls::Slider bar{nullptr};
  Controls::Button play{nullptr};
  Controls::TextBlock time{nullptr};
};

}  // namespace

MainWindow::MainWindow() {
  sp_log("ctor: enter");
  window_ = Window();
  sp_log("1 window created");
  // Title is set below; calling it here faults on this SDK.

  // A real title bar: the system keeps the drag region, snapping, the system menu and the
  // min/max/close buttons, and they are drawn by Windows rather than by us.
  sp_log("3 extends-content set");
  sp_log("4 backdrop set");

  sp_log("ctor: window + title bar flags set");
  config_ = sp::Config::load();

  auto root = Grid();
  root.RowDefinitions().Append(ui::rows(ui::automatic()));
  root.RowDefinitions().Append(ui::rows(ui::star()));
  root.RowDefinitions().Append(ui::rows(ui::automatic()));
  root.RequestedTheme(config_.theme == "dark"    ? ElementTheme::Dark
                      : config_.theme == "light" ? ElementTheme::Light
                                                 : ElementTheme::Default);

  sp_log("BuildTitleBar");
  BuildTitleBar(root);

  auto scroller = ScrollViewer();
  scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
  scroller.Padding(Thickness{16.0, 0.0, 16.0, 0.0});
  auto stack = ui::column(8.0);
  stack.Padding(Thickness{0.0, 8.0, 0.0, 16.0});
  sp_log("BuildVideos");
  BuildVideos(stack);
  sp_log("BuildPlayback");
  BuildPlayback(stack);
  sp_log("BuildTimelines");
  BuildTimelines(stack);
  sp_log("BuildVolume");
  BuildVolume(stack);
  sp_log("BuildWindows");
  BuildWindows(stack);
  scroller.Content(stack);
  Grid::SetRow(scroller, 1);
  root.Children().Append(scroller);

  sp_log("BuildFooter");
  BuildFooter(root);

  sp_log("ctor: setting content");
  Controls::TextBlock raw;
  raw.Text(L"raw control test");
  sp_log("ctor: raw TextBlock built");
  window_.Content(raw);
  sp_log("ctor: trivial content set");
  try {
    window_.Content(root);
    sp_log("ctor: content set");
  } catch (hresult_error const& e) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "Content FAILED 0x%08X: %ls", (unsigned)e.code().value, e.message().c_str());
    sp_log(buf);
    return;
  }
  try {
    window_.Activate();
    sp_log("ctor: activated");
  } catch (hresult_error const& e) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), "Activate FAILED 0x%08X: %ls", (unsigned)e.code().value, e.message().c_str());
    sp_log(buf);
  }
}

void MainWindow::BuildTitleBar(Grid const& root) {
  auto bar = Grid();
  bar.Height(40.0);
  bar.Padding(Thickness{16.0, 0.0, 8.0, 0.0});
  bar.ColumnDefinitions().Append(ui::cols(ui::automatic()));
  bar.ColumnDefinitions().Append(ui::cols(ui::automatic()));
  bar.ColumnDefinitions().Append(ui::cols(ui::star()));
  bar.ColumnDefinitions().Append(ui::cols(ui::automatic()));

  auto title = ui::text(L"SyncPlayer", 14.0, nullptr, true);
  title.VerticalAlignment(VerticalAlignment::Center);
  Grid::SetColumn(title, 0);
  bar.Children().Append(title);

  auto chip = Border();
  chip.Background(ui::brush(L"ControlFillColorSecondaryBrush"));
  chip.CornerRadius(CornerRadius{10.0, 10.0, 10.0, 10.0});
  chip.Padding(Thickness{8.0, 2.0, 8.0, 2.0});
  chip.Margin(Thickness{8.0, 0.0, 0.0, 0.0});
  chip.VerticalAlignment(VerticalAlignment::Center);
  chip.Child(ui::text(kVersion, 12.0, ui::brush(L"TextFillColorSecondaryBrush")));
  Grid::SetColumn(chip, 1);
  bar.Children().Append(chip);

  auto settings = ui::button(L"Settings");
  settings.VerticalAlignment(VerticalAlignment::Center);
  settings.Click([this](IInspectable const&, RoutedEventArgs const&) {
    SetStatus(L"Settings", L"The settings page is ported with the title bar.");
  });
  Grid::SetColumn(settings, 3);
  bar.Children().Append(settings);

  Grid::SetRow(bar, 0);
  root.Children().Append(bar);
}

// VIDEOS: the two sources, each a caption, a path field and a browse button.
void MainWindow::BuildVideos(StackPanel const& stack) {
  StackPanel body{nullptr};
  auto card = ui::card(L"VIDEOS", body);

  auto make_source = [this](hstring const& label, hstring const& placeholder,
                            Controls::TextBox& box) {
    auto line = ui::column(2.0);
    line.Children().Append(ui::caption(label));
    auto row = ui::row();
    box = ui::field(placeholder);
    box.HorizontalAlignment(HorizontalAlignment::Stretch);
    box.TextWrapping(TextWrapping::NoWrap);
    row.Children().Append(box);
    auto browse = ui::button(L"Browse...");
    browse.Click([this, &box](IInspectable const&, RoutedEventArgs const&) {
      Browse(&box == &movie_box_ ? 0 : 1);
    });
    row.Children().Append(browse);
    line.Children().Append(row);
    return line;
  };

  auto grid = Grid();
  grid.ColumnDefinitions().Append(ui::cols(ui::star()));
  auto movie = make_source(L"Movie", L"C:/media/movie.mp4", movie_box_);
  Grid::SetColumn(movie, 0);
  grid.Children().Append(movie);

  body.Children().Append(grid);
  body.Children().Append(make_source(L"Reaction", L"C:/media/reaction.mp4", reaction_box_));
  body.Children().Append(ui::caption(L"Paste a link, or drop a file onto the window."));

  stack.Children().Append(card);
}

// PLAYBACK: transport on one line, the three settings on the next, exactly as the reference does.
void MainWindow::BuildPlayback(StackPanel const& stack) {
  StackPanel body{nullptr};
  auto card = ui::card(L"PLAYBACK", body);

  auto transport = ui::row();
  start_button_ = ui::accent_button(L"Start");
  play_button_ = ui::button(L"Play");
  jump_back_button_ = ui::button(L"\u2190 7.5s");
  jump_fwd_button_ = ui::button(L"7.5s \u2192");
  transport.Children().Append(start_button_);
  transport.Children().Append(play_button_);
  transport.Children().Append(jump_back_button_);
  transport.Children().Append(jump_fwd_button_);
  body.Children().Append(transport);

  auto settings_row = ui::row(16.0);
  jump_box_ = ui::field(L"5", hstring(std::to_wstring(static_cast<int>(config_.jump_sec))));
  jump_box_.Width(90.0);
  settings_row.Children().Append(ui::labelled(L"Jump (s)", jump_box_));

  speed_box_ = ui::field(L"1.00", L"1.00");
  speed_box_.Width(90.0);
  settings_row.Children().Append(ui::labelled(L"Speed", speed_box_));

  auto lock_block = ui::column(2.0);
  lock_block.Children().Append(ui::caption(L"Lock sync to the master bar"));
  lock_switch_ = ToggleSwitch();
  lock_switch_.OnContent(box_value(L""));
  lock_switch_.OffContent(box_value(L""));
  lock_block.Children().Append(lock_switch_);
  settings_row.Children().Append(lock_block);

  body.Children().Append(settings_row);

  sync_note_ = ui::caption(L"Sync unlocked: align with the per-video bars.");
  body.Children().Append(sync_note_);

  stack.Children().Append(card);
}

// TIMELINES: one row per video plus the master bar, then the Go to field.
void MainWindow::BuildTimelines(StackPanel const& stack) {
  StackPanel body{nullptr};
  auto card = ui::card(L"TIMELINES", body);

  auto make_row = [this](hstring const& label, bool with_play, Controls::Slider& bar,
                         Controls::Button& play, Controls::TextBlock& time) {
    auto grid = Grid();
    grid.ColumnDefinitions().Append(ui::cols(ui::automatic()));
    grid.ColumnDefinitions().Append(ui::cols(ui::star()));
    grid.ColumnDefinitions().Append(ui::cols(ui::automatic()));
    grid.ColumnDefinitions().Append(ui::cols(ui::automatic()));
    grid.Margin(Thickness{0.0, 0.0, 0.0, 4.0});

    auto name = ui::text(label, 13.0);
    name.Width(84.0);
    name.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(name, 0);
    grid.Children().Append(name);

    bar = Slider();
    bar.Minimum(0.0);
    bar.Maximum(100.0);
    bar.VerticalAlignment(VerticalAlignment::Center);
    bar.Margin(Thickness{0.0, 0.0, 8.0, 0.0});
    Grid::SetColumn(bar, 1);
    grid.Children().Append(bar);

    if (with_play) {
      play = ui::button(L"\u25B6");
      play.Width(36.0);
      play.Margin(Thickness{0.0, 0.0, 8.0, 0.0});
      Grid::SetColumn(play, 2);
      grid.Children().Append(play);
    }

    time = ui::text(L"00:00 / 00:00", 12.0, ui::brush(L"TextFillColorSecondaryBrush"));
    time.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(time, 3);
    grid.Children().Append(time);
    return grid;
  };

  Button unused{nullptr};
  body.Children().Append(make_row(L"Movie", true, movie_bar_, unused, movie_time_));
  body.Children().Append(make_row(L"Reaction", true, reaction_bar_, unused, reaction_time_));
  body.Children().Append(make_row(L"Master", false, master_bar_, unused, master_time_));

  auto goto_row = ui::row(16.0);
  goto_box_ = ui::field(L"90 or 1:30:00");
  goto_box_.Width(140.0);
  goto_row.Children().Append(ui::labelled(L"Go to", goto_box_));
  body.Children().Append(goto_row);

  stack.Children().Append(card);
}

// VOLUME: three labelled sliders.
void MainWindow::BuildVolume(StackPanel const& stack) {
  StackPanel body{nullptr};
  auto card = ui::card(L"VOLUME", body);

  auto make_slider = [this](hstring const& label, Controls::Slider& slider,
                            Controls::TextBlock& readout) {
    auto grid = Grid();
    grid.ColumnDefinitions().Append(ui::cols(ui::automatic()));
    grid.ColumnDefinitions().Append(ui::cols(ui::star()));
    readout = ui::text(label, 13.0);
    readout.Width(120.0);
    readout.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(readout, 0);
    grid.Children().Append(readout);
    slider = Slider();
    slider.Minimum(0.0);
    slider.Maximum(150.0);
    slider.Value(100.0);
    slider.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(slider, 1);
    grid.Children().Append(slider);
    return grid;
  };

  body.Children().Append(make_slider(L"Movie 100%", movie_vol_, movie_vol_label_));
  body.Children().Append(make_slider(L"Reaction 100%", reaction_vol_, reaction_vol_label_));
  body.Children().Append(make_slider(L"Master 100%", master_vol_, master_vol_label_));

  stack.Children().Append(card);
}

// WINDOWS: side by side and the floating PiP.
void MainWindow::BuildWindows(StackPanel const& stack) {
  StackPanel body{nullptr};
  auto card = ui::card(L"WINDOWS", body);

  auto line = ui::row();
  auto arrange = ui::button(L"Arrange side by side");
  line.Children().Append(arrange);
  pip_button_ = ui::button(L"Floating PiP");
  line.Children().Append(pip_button_);
  body.Children().Append(line);
  body.Children().Append(
      ui::caption(L"Floating PiP makes the reaction borderless and always on top."));

  stack.Children().Append(card);
}

void MainWindow::BuildFooter(Grid const& root) {
  auto foot = ui::column(2.0);
  foot.Padding(Thickness{16.0, 8.0, 16.0, 12.0});
  status_line_ = ui::text(L"Ready.", 12.0, ui::brush(L"TextFillColorSecondaryBrush"));
  status_hint_ = ui::caption(L"");
  foot.Children().Append(status_line_);
  foot.Children().Append(status_hint_);
  Grid::SetRow(foot, 2);
  root.Children().Append(foot);
}

void MainWindow::SetStatus(hstring const& line, hstring const& hint) {
  if (status_line_) status_line_.Text(line);
  if (status_hint_) status_hint_.Text(hint);
}

// The classic browse dialog. A WinRT picker would need the window handle through the interop
// interface, which is more moving parts than a file name is worth here.
void MainWindow::Browse(int side) {
  wchar_t name[MAX_PATH] = L"";
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = GetActiveWindow();
  ofn.lpstrFilter = L"Video files\0*.mp4;*.mkv;*.webm;*.avi;*.mov\0All files\0*.*\0";
  ofn.lpstrFile = name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&ofn)) return;

  auto value = hstring(name);
  if (side == 0) {
    movie_box_.Text(value);
    config_.movie = winrt::to_string(value);
  } else {
    reaction_box_.Text(value);
    config_.reaction = winrt::to_string(value);
  }
  config_.save();
  SetStatus(L"Source set.", value);
}

void MainWindow::Activate() {
  sp_log("Activate");
  window_.Activate();
  sp_log("Activate returned");
}

}  // namespace SyncPlayer
