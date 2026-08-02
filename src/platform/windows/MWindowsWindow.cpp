#include "MWindowsWindow.h"

#include <winrt/Microsoft.UI.Interop.h>

#include "MWindowsPlatform.h"
#include "Mulu/MButton.h"
#include "Mulu/MLabel.h"
#include "Mulu/MStackLayout.h"
#include "Mulu/MWidget.h"
#include "Mulu/MWindow.h"

namespace mulu {

namespace {

// UTF-8 MString -> WinRT hstring.
winrt::hstring toHstring(const MString& text)
{
    return winrt::hstring(text.toStdWString());
}

} // namespace

MWindowsWindow::MWindowsWindow(MWindow* owner)
    : m_owner(owner)
{
    if (owner) {
        m_title = owner->title();
        m_size = owner->size();
    }
}

MWindowsWindow::~MWindowsWindow()
{
    // Drop every native control and the XAML window *before* the M-widgets
    // they reference are deleted (MWindow tears down this object first).
    m_buttonMap.clear();
    m_labelMap.clear();
    m_rootPanel = nullptr;
    m_window = nullptr;
    m_nativeHandle = nullptr;
}

void MWindowsWindow::show()
{
    m_visible = true;
    ensureCreated();
    if (m_window) {
        m_window.Activate();
        m_pendingShow = false;
    } else {
        // The XAML runtime is not up yet; OnLaunched will activate us.
        m_pendingShow = true;
    }
}

void MWindowsWindow::hide()
{
    m_visible = false;
    m_pendingShow = false;
    if (m_window) {
        // WinUI3 has no public Hide(); closing the XAML window is the
        // supported way to take it down.
        m_window.Close();
    }
}

void MWindowsWindow::close()
{
    if (m_window) {
        m_window.Close();
    }
}

void MWindowsWindow::activate()
{
    if (m_window) {
        m_window.Activate();
        m_pendingShow = false;
    }
}

void MWindowsWindow::ensureCreated()
{
    if (m_window) {
        return;
    }
    const MWindowsPlatform* platform = MWindowsPlatform::instance();
    if (!platform || !platform->isXamlStarted()) {
        return; // cannot create XAML windows before the event loop is running
    }
    createXamlWindow();
}

void MWindowsWindow::createXamlWindow()
{
    m_window = winrt::Microsoft::UI::Xaml::Window();
    m_window.Title(toHstring(m_title));

    m_window.AppWindow().Resize(winrt::Windows::Graphics::SizeInt32{
        static_cast<int32_t>(m_size.width()),
        static_cast<int32_t>(m_size.height())});

    m_rootPanel = winrt::Microsoft::UI::Xaml::Controls::StackPanel();
    m_window.Content(m_rootPanel);

    if (m_owner && m_owner->centralWidget()) {
        syncWidgetTree(m_owner->centralWidget());
    }

    // Expose the backing Win32 HWND through the native handle API.
    m_nativeHandle = reinterpret_cast<void*>(
        winrt::Microsoft::UI::Interop::WindowNative::GetWindowHandle(m_window));
}

void MWindowsWindow::syncWidgetTree(MWidget* root)
{
    if (!m_window || !m_rootPanel) {
        return;
    }
    m_buttonMap.clear();
    m_labelMap.clear();
    m_rootPanel.Children().Clear();
    if (root) {
        m_rootPanel.Children().Append(toXamlElement(root));
    }
}

winrt::Microsoft::UI::Xaml::Controls::UIElement MWindowsWindow::toXamlElement(MWidget* widget)
{
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    if (auto* button = dynamic_cast<MButton*>(widget)) {
        auto native = Button();
        native.Content(winrt::box_value(toHstring(button->text())));
        native.Click([this, button](winrt::Windows::Foundation::IInspectable const&,
                                    winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) {
            button->click();
        });
        m_buttonMap[native] = button;
        return native;
    }

    if (auto* label = dynamic_cast<MLabel*>(widget)) {
        auto native = TextBlock();
        native.Text(toHstring(label->text()));
        native.FontSize(16.0);
        m_labelMap[native] = label;
        return native;
    }

    if (auto* layout = dynamic_cast<MStackLayout*>(widget)) {
        auto native = StackPanel();
        native.Orientation(
            layout->orientation() == MStackLayout::Orientation::Horizontal
                ? Orientation::Horizontal
                : Orientation::Vertical);
        native.Spacing(8.0);
        for (MWidget* child : layout->widgets()) {
            native.Children().Append(toXamlElement(child));
        }
        return native;
    }

    // Fallback: a plain MWidget acts as a container. Render any MStackLayout
    // children it owns (e.g. a Qt Designer 'centralwidget' holding a layout).
    {
        auto native = StackPanel();
        native.Spacing(8.0);
        for (MObject* child : widget->children()) {
            if (auto* layout = dynamic_cast<MStackLayout*>(child)) {
                native.Children().Append(toXamlElement(layout));
            }
        }
        if (native.Children().Size() > 0) {
            return native;
        }
    }
    return Grid();
}

void MWindowsWindow::setTitle(const MString& title)
{
    m_title = title;
    if (m_window) {
        m_window.Title(toHstring(m_title));
    }
}

void MWindowsWindow::setSize(const MSize& size)
{
    m_size = size;
    if (m_window) {
        m_window.AppWindow().Resize(winrt::Windows::Graphics::SizeInt32{
            static_cast<int32_t>(m_size.width()),
            static_cast<int32_t>(m_size.height())});
    }
}

void* MWindowsWindow::nativeHandle()
{
    return m_nativeHandle;
}

} // namespace mulu
