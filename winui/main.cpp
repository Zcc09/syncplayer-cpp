// SyncPlayer, WinUI 3, C++/WinRT.
//
// Application::Start needs an Application subclass; the window itself lives in MainWindow.
#include "pch.h"

#include "MainWindow.h"
#include "log.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Markup;
using namespace Microsoft::UI::Xaml::XamlTypeInfo;
using namespace Windows::UI::Xaml::Interop;

namespace SyncPlayer {

// Also the XAML type resolver: WinUI resolves its own control styles through the metadata
// provider, and without it every control renders in the old UWP style instead.
struct App : ApplicationT<App, winrt::Microsoft::UI::Xaml::Markup::IXamlMetadataProvider> {
  void OnLaunched(LaunchActivatedEventArgs const&) {
    // Before the window: it looks its colours up from these resources.
    sp_log("OnLaunched: enter");
    Resources().MergedDictionaries().Append(XamlControlsResources());
    sp_log("OnLaunched: theme resources merged");
    window_ = std::make_unique<MainWindow>();
    sp_log("OnLaunched: window constructed");
    window_->Activate();
  }

  winrt::Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(TypeName const& type) { return provider_.GetXamlType(type); }
  winrt::Microsoft::UI::Xaml::Markup::IXamlType GetXamlType(hstring const& fullname) { return provider_.GetXamlType(fullname); }
  com_array<winrt::Microsoft::UI::Xaml::Markup::XmlnsDefinition> GetXmlnsDefinitions() { return provider_.GetXmlnsDefinitions(); }

 private:
  std::unique_ptr<MainWindow> window_;
  XamlControlsXamlMetaDataProvider provider_;
};

}  // namespace SyncPlayer

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  init_apartment();
  Application::Start([](auto&&) { make<SyncPlayer::App>(); });
  return 0;
}
