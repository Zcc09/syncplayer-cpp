#include "pch.h"
#include "MainWindow.xaml.h"

#include <commdlg.h>   // GetOpenFileNameW: a browse dialog with no window interop needed

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;

namespace SyncPlayer {

MainWindow::MainWindow() {
  InitializeComponent();

  // A real title bar: the system owns the drag region, snapping, the system menu and the
  // min/max/close buttons, and it draws them itself.
  ExtendsContentIntoTitleBar(true);
  SetTitleBar(TitleBar());
  Title(L"SyncPlayer");
  SystemBackdrop(MicaBackdrop{});

  config_ = sp::Config::load();

  // The controls start from the stored settings, so the port reopens where the old build left off.
  MovieBox().Text(to_hstring(config_.movie));
  ReactionBox().Text(to_hstring(config_.reaction));
  SpeedBox().Text(to_hstring(config_.speed));
  JumpBox().Text(to_hstring(static_cast<int>(config_.jump_sec)));
  MovieVolume().Value(config_.vol_a);
  ReactionVolume().Value(config_.vol_b);
  MasterVolume().Value(config_.vol_m);

  if (config_.theme == "dark") {
    Root().RequestedTheme(ElementTheme::Dark);
  } else if (config_.theme == "light") {
    Root().RequestedTheme(ElementTheme::Light);
  }

  SetStatus(L"Ready.", L"Choose the movie and the reaction, then press Start.");
}

void MainWindow::SetStatus(hstring const& line, hstring const& hint) {
  StatusLine().Text(line);
  StatusHint().Text(hint);
}

// The classic browse dialog. A WinRT picker would need the window handle through an interop
// interface, which is more moving parts than choosing a file is worth.
void MainWindow::Browse(TextBox const& box, int side) {
  wchar_t name[MAX_PATH] = L"";
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = GetActiveWindow();
  ofn.lpstrFilter = L"Video files\0*.mp4;*.mkv;*.webm;*.avi;*.mov\0All files\0*.*\0";
  ofn.lpstrFile = name;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&ofn)) return;

  box.Text(hstring(name));
  if (side == 0) {
    config_.movie = to_string(hstring(name));
  } else {
    config_.reaction = to_string(hstring(name));
  }
  config_.save();
  SetStatus(L"Source set.", hstring(name));
}

// The handlers: the layout and the wiring to the core come next, so each one reports what it
// would do rather than doing nothing at all.
void MainWindow::OnSettingsClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Settings", L"Appearance, status bar and shortcuts move here from the title bar.");
}

void MainWindow::OnBrowseMovieClicked(IInspectable const&, RoutedEventArgs const&) {
  Browse(MovieBox(), 0);
}

void MainWindow::OnBrowseReactionClicked(IInspectable const&, RoutedEventArgs const&) {
  Browse(ReactionBox(), 1);
}

void MainWindow::OnStartClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Start", L"Launches both players and syncs them to the master bar.");
}

void MainWindow::OnPlayClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Play", L"Plays or pauses both videos.");
}

void MainWindow::OnJumpBackClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Jump back", L"Rewinds both videos by the jump amount.");
}

void MainWindow::OnJumpForwardClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Jump forward", L"Advances both videos by the jump amount.");
}

void MainWindow::OnLockToggled(IInspectable const&, RoutedEventArgs const&) {
  const bool locked = LockSwitch().IsOn();
  SyncNote().Text(locked ? L"Sync locked: the Master bar drives both videos."
                         : L"Sync unlocked: align with the per-video bars.");
}

void MainWindow::OnPlayMovieClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Movie", L"Plays or pauses the movie, or both when sync is locked.");
}

void MainWindow::OnPlayReactionClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Reaction", L"Plays or pauses the reaction, or both when sync is locked.");
}

void MainWindow::OnArrangeClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Arrange side by side", L"Puts the two player windows side by side.");
}

void MainWindow::OnPipClicked(IInspectable const&, RoutedEventArgs const&) {
  SetStatus(L"Floating PiP", L"Makes the reaction borderless and always on top.");
}

}  // namespace SyncPlayer
