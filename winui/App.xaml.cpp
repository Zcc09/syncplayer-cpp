#include "pch.h"
#include "App.xaml.h"

#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace SyncPlayer {

void App::OnLaunched(LaunchActivatedEventArgs const&) {
  window_ = MainWindow();
  window_.Activate();
}

}  // namespace SyncPlayer
