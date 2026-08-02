#include "MWinUIApp.h"

#include "MWindowsPlatform.h"

namespace mulu {

MWinUIApp::MWinUIApp()
{
    // Reserved: app-wide resources / theme overrides can be installed here.
}

void MWinUIApp::OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const& /*args*/)
{
    if (MWindowsPlatform* platform = MWindowsPlatform::instance()) {
        platform->onXamlApplicationStarted();
    }
}

} // namespace mulu
