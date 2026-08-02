#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <winrt/Microsoft.UI.Xaml.h>

#include "Mulu/MPlatform.h"
#include "Mulu/MString.h"

namespace mulu {

class MWindow;

// WinUI3 (Windows App SDK) backend for MuluUI.
//
// Responsibilities:
//   - bootstrap the Windows App Runtime (unpackaged deployment)
//   - start the WinUI3 XAML event loop via Application::Start
//   - own all MWindowsWindow instances and bridge them to the XAML runtime
class MWindowsPlatform : public MPlatformBackend {
public:
    MWindowsPlatform();
    ~MWindowsPlatform() override;

    bool initialize(int argc, char** argv) override;
    int runEventLoop() override;
    void quit() override;

    MPlatformWindow* createWindow(MWindow* owner) override;
    void destroyWindow(MPlatformWindow* window) override;

    void setApplicationName(const MString& name) override { m_appName = name; }
    MString applicationName() const override { return m_appName; }

    // Singleton access for the WinUI shim objects (MWinUIApp).
    static MWindowsPlatform* instance();

    // True once the XAML runtime is up and native windows can be created.
    bool isXamlStarted() const { return m_xamlStarted; }

    // Called by MWinUIApp::OnLaunched once the XAML runtime is ready.
    void onXamlApplicationStarted();

    // Keeps the top-level WinUI Application object alive for the loop and
    // lets quit() ask it to exit.
    void storeApplication(winrt::Microsoft::UI::Xaml::Application const& app);

private:
    MString m_appName = "MuluApplication";
    int m_exitCode = 0;
    bool m_xamlStarted = false;
    bool m_quitRequested = false;
    std::vector<std::unique_ptr<MPlatformWindow>> m_windows;
    std::recursive_mutex m_mutex;
    winrt::Microsoft::UI::Xaml::Application m_app{nullptr};

    static MWindowsPlatform* s_instance;
};

} // namespace mulu
