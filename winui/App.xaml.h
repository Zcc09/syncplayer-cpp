#pragma once

#include "pch.h"
#include "App.g.h"

namespace SyncPlayer {

// The application object. With XAML in the project the framework generates the base class, which
// is what makes the window, its title bar and its controls work the supported way - the previous
// hand-built equivalent could not attach content at all.
struct App : AppT<App> {
  App() = default;

  void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

 private:
  winrt::Microsoft::UI::Xaml::Window window_{nullptr};
};

}  // namespace SyncPlayer
