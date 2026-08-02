#include "MWindowsPlatform.h"

#include <algorithm>

#include <MddBootstrap.h>
#include <WindowsAppSDK-VersionInfo.h>
#include <windows.h>

#include "MWindowsWindow.h"
#include "MWinUIApp.h"
#include "Mulu/MWindow.h"

namespace mulu {

MWindowsPlatform* MWindowsPlatform::s_instance = nullptr;

MWindowsPlatform::MWindowsPlatform()
{
    s_instance = this;
}

MWindowsPlatform::~MWindowsPlatform()
{
    m_app = nullptr;
    if (s_instance == this) {
        s_instance = nullptr;
    }
    MddBootstrapShutdown();
}

MWindowsPlatform* MWindowsPlatform::instance()
{
    return s_instance;
}

bool MWindowsPlatform::initialize(int /*argc*/, char** /*argv*/)
{
    // Initialize the Windows App Runtime for unpackaged applications.
    // MddBootstrapInitialize must complete before Application::Start runs.
    const HRESULT hr = MddBootstrapInitialize(MICROSOFT_WINDOWSAPPSDK_RELEASE_MAJOR);
    return SUCCEEDED(hr);
}

int MWindowsPlatform::runEventLoop()
{
    m_xamlStarted = false;

    // Application::Start blocks until the app shuts down. The callback
    // creates the top-level WinUI Application object; the runtime then
    // invokes MWinUIApp::OnLaunched, where we finally materialize the
    // native windows that were requested before exec().
    winrt::Microsoft::UI::Xaml::Application::Start([]() {
        auto app = winrt::make<MWinUIApp>();
        if (MWindowsPlatform* platform = MWindowsPlatform::instance()) {
            platform->storeApplication(app);
        }
    });

    m_xamlStarted = false;
    return m_exitCode;
}

void MWindowsPlatform::quit()
{
    m_quitRequested = true;
    if (m_app) {
        // Ask the WinUI event loop to terminate.
        m_app.Exit();
    }
}

void MWindowsPlatform::storeApplication(winrt::Microsoft::UI::Xaml::Application const& app)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_app = app;
}

MPlatformWindow* MWindowsPlatform::createWindow(MWindow* owner)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto window = std::make_unique<MWindowsWindow>(owner);
    MPlatformWindow* raw = window.get();
    m_windows.push_back(std::move(window));
    return raw;
}

void MWindowsPlatform::destroyWindow(MPlatformWindow* window)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_windows.erase(
        std::remove_if(
            m_windows.begin(), m_windows.end(),
            [window](const std::unique_ptr<MPlatformWindow>& w) {
                return w.get() == window;
            }),
        m_windows.end());
}

void MWindowsPlatform::onXamlApplicationStarted()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_xamlStarted = true;

    // Materialize any native windows requested before the event loop started
    // and activate those that were already shown.
    for (const auto& w : m_windows) {
        if (auto* win = dynamic_cast<MWindowsWindow*>(w.get())) {
            win->ensureCreated();
            if (win->pendingShow()) {
                win->activate();
            }
        }
    }
}

} // namespace mulu
