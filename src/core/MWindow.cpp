#include "Mulu/MWindow.h"

#include "Mulu/MApplication.h"
#include "Mulu/MPlatform.h"
#include "Mulu/MWidget.h"

namespace mulu {

MWindow::MWindow(MWindow* parent)
    : MObject(parent)
{
}

MWindow::~MWindow()
{
    // Tear down the native window before the widget tree is destroyed
    // (the base MObject destructor deletes the children afterwards).
    if (m_platformWindow) {
        if (MApplication* app = MApplication::instance()) {
            if (MPlatformBackend* backend = app->backend()) {
                backend->destroyWindow(m_platformWindow);
            }
        }
        m_platformWindow = nullptr;
    }
}

void MWindow::ensurePlatformWindow()
{
    if (m_platformWindow) {
        return;
    }
    if (MApplication* app = MApplication::instance()) {
        if (MPlatformBackend* backend = app->backend()) {
            m_platformWindow = backend->createWindow(this);
        }
    }
}

void MWindow::show()
{
    ensurePlatformWindow();
    if (m_platformWindow) {
        m_platformWindow->show();
    }
}

void MWindow::hide()
{
    if (m_platformWindow) {
        m_platformWindow->hide();
    }
}

void MWindow::close()
{
    if (m_platformWindow) {
        m_platformWindow->close();
    }
    handleClose();
}

void MWindow::handleClose()
{
    if (m_onClose) {
        m_onClose();
    }
    hide();
}

void MWindow::setTitle(const MString& title)
{
    m_title = title;
    if (m_platformWindow) {
        m_platformWindow->setTitle(title);
    }
}

void MWindow::setSize(const MSize& size)
{
    m_size = size;
    if (m_platformWindow) {
        m_platformWindow->setSize(size);
    }
}

void MWindow::setCentralWidget(MWidget* widget)
{
    if (m_centralWidget == widget) {
        return;
    }
    m_centralWidget = widget;
    if (widget) {
        widget->setParent(this);
    }
    requestWidgetSync(widget);
}

void MWindow::requestWidgetSync(MWidget* /*widget*/)
{
    if (m_platformWindow && m_centralWidget) {
        m_platformWindow->syncWidgetTree(m_centralWidget);
    }
}

void* MWindow::nativeHandle() const
{
    return m_platformWindow ? m_platformWindow->nativeHandle() : nullptr;
}

} // namespace mulu
