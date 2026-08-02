#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

namespace mulu {

// The top-level WinUI3 Application object (the bridge between the Windows
// App SDK runtime and MuluUI). Once launched, it notifies MWindowsPlatform
// so that pending MWindow instances can be materialized as XAML windows.
struct MWinUIApp : winrt::Microsoft::UI::Xaml::ApplicationT<MWinUIApp>
{
    MWinUIApp();

    void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const& args);
};

} // namespace mulu
