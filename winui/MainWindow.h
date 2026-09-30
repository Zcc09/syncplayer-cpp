// The SyncPlayer window, WinUI 3.
//
// Same layout as the hand-drawn build: a title bar with the app name, version chip and Settings,
// then the stacked groups - Videos, Playback, Timelines, Volume, Windows - and a status footer.
// The controls are real WinUI controls, so focus, keyboard, touch, theme and high contrast all
// behave the way Windows expects.
#pragma once

#include "pch.h"

#include "core/config.h"
#include "core/mpv.h"

namespace SyncPlayer {

class MainWindow {
 public:
  MainWindow();

  void Activate();
  winrt::Microsoft::UI::Xaml::Window Window() const { return window_; }

 private:
  void BuildTitleBar(winrt::Microsoft::UI::Xaml::Controls::Grid const& root);
  void BuildVideos(winrt::Microsoft::UI::Xaml::Controls::StackPanel const& stack);
  void BuildPlayback(winrt::Microsoft::UI::Xaml::Controls::StackPanel const& stack);
  void BuildTimelines(winrt::Microsoft::UI::Xaml::Controls::StackPanel const& stack);
  void BuildVolume(winrt::Microsoft::UI::Xaml::Controls::StackPanel const& stack);
  void BuildWindows(winrt::Microsoft::UI::Xaml::Controls::StackPanel const& stack);
  void BuildFooter(winrt::Microsoft::UI::Xaml::Controls::Grid const& root);

  void Browse(int side);
  void SetStatus(winrt::hstring const& line, winrt::hstring const& hint);

  winrt::Microsoft::UI::Xaml::Window window_{nullptr};
  HWND hwnd_{nullptr};

  // Controls the runtime updates after creation.
  winrt::Microsoft::UI::Xaml::Controls::TextBox movie_box_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBox reaction_box_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button start_button_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button play_button_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button jump_back_button_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button jump_fwd_button_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBox jump_box_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBox speed_box_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch lock_switch_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock sync_note_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Slider movie_bar_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Slider reaction_bar_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Slider master_bar_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock movie_time_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock reaction_time_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock master_time_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBox goto_box_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Slider movie_vol_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Slider reaction_vol_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Slider master_vol_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock movie_vol_label_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock reaction_vol_label_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock master_vol_label_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::Button pip_button_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock status_line_{nullptr};
  winrt::Microsoft::UI::Xaml::Controls::TextBlock status_hint_{nullptr};

  // Core state, shared with the CMake build through src/core.
  sp::Config config_;
};

}  // namespace SyncPlayer
