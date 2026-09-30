#pragma once

#include "pch.h"

#include "MainWindow.g.h"

#include "core/config.h"

namespace SyncPlayer {

// The app window. The layout is in MainWindow.xaml; this is the code-behind: window chrome,
// the core config, and the click handlers.
struct MainWindow : MainWindowT<MainWindow> {
  MainWindow();

  void OnSettingsClicked(winrt::Windows::Foundation::IInspectable const&,
                         winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnBrowseMovieClicked(winrt::Windows::Foundation::IInspectable const&,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnBrowseReactionClicked(winrt::Windows::Foundation::IInspectable const&,
                               winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnStartClicked(winrt::Windows::Foundation::IInspectable const&,
                      winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnPlayClicked(winrt::Windows::Foundation::IInspectable const&,
                     winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnJumpBackClicked(winrt::Windows::Foundation::IInspectable const&,
                         winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnJumpForwardClicked(winrt::Windows::Foundation::IInspectable const&,
                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnLockToggled(winrt::Windows::Foundation::IInspectable const&,
                     winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnPlayMovieClicked(winrt::Windows::Foundation::IInspectable const&,
                          winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnPlayReactionClicked(winrt::Windows::Foundation::IInspectable const&,
                             winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnArrangeClicked(winrt::Windows::Foundation::IInspectable const&,
                        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
  void OnPipClicked(winrt::Windows::Foundation::IInspectable const&,
                    winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);

 private:
  void Browse(winrt::Microsoft::UI::Xaml::Controls::TextBox const& box, int side);
  void SetStatus(winrt::hstring const& line, winrt::hstring const& hint = L"");

  sp::Config config_;
};

}  // namespace SyncPlayer
