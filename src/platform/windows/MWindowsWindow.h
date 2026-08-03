#pragma once

#include <unordered_map>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
// The SDK 10.0.26100.0 (VS2026-era) C++/WinRT headers moved the consume_*
// method definitions out of the impl (*.0.h/*.2.h) headers and into the main
// namespace headers, and moved winrt::box_value out of base.h into
// Windows.Foundation.h. Include the main headers explicitly so the methods
// used below (Button::Click via IButtonBase, IVector<T> Clear/Append/Size,
// winrt::box_value) have complete definitions.
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.h>

#include "Mulu/MPlatform.h"
#include "Mulu/MSize.h"
#include "Mulu/MString.h"

namespace mulu {

class MWindow;
class MWidget;
class MButton;
class MLabel;
class MStackLayout;

// WinUI3-backed native window bound to a logical MWindow.
//
// XAML windows cannot be created before the WinUI3 event loop is running
// (Application::Start), so creation is deferred until ensureCreated() is
// called by the platform once the runtime is up.
class MWindowsWindow : public MPlatformWindow {
public:
    explicit MWindowsWindow(MWindow* owner);
    ~MWindowsWindow() override;

    void show() override;
    void hide() override;
    void close() override;

    void setTitle(const MString& title) override;
    MString title() const override { return m_title; }

    void setSize(const MSize& size) override;
    MSize size() const override { return m_size; }

    void* nativeHandle() override;

    void syncWidgetTree(MWidget* root) override;

    // Called by the platform once the XAML runtime is available.
    void ensureCreated();
    void activate();
    bool pendingShow() const { return m_pendingShow; }
    bool isCreated() const { return m_window != nullptr; }

private:
    void createXamlWindow();
    winrt::Microsoft::UI::Xaml::UIElement toXamlElement(MWidget* widget);

    MWindow* m_owner = nullptr;
    MString m_title;
    MSize m_size{800, 600};
    bool m_visible = false;
    bool m_pendingShow = false;
    void* m_nativeHandle = nullptr;

    winrt::Microsoft::UI::Xaml::Window m_window{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::StackPanel m_rootPanel{nullptr};

    // Keep the native controls alive and bridge events back to the M-widgets.
    std::unordered_map<winrt::Microsoft::UI::Xaml::Controls::Button, MButton*> m_buttonMap;
    std::unordered_map<winrt::Microsoft::UI::Xaml::Controls::TextBlock, MLabel*> m_labelMap;
};

} // namespace mulu
