#include "Mulu/MWidget.h"

#include "Mulu/MWindow.h"

namespace mulu {

MWidget::MWidget(MWidget* parent)
    : MObject(parent)
{
}

MWidget::~MWidget() = default;

void MWidget::setGeometry(const MRect& geometry)
{
    m_geometry = geometry;
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MWidget::setPosition(const MPoint& position)
{
    m_geometry.setPosition(position);
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MWidget::setSize(const MSize& size)
{
    m_geometry.setSize(size);
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MWidget::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
    if (m_visible) {
        onShown();
    } else {
        onHidden();
    }
}

void MWidget::show()
{
    setVisible(true);
}

void MWidget::hide()
{
    setVisible(false);
}

MWindow* MWidget::window() const
{
    const MObject* object = this;
    while (object) {
        if (const auto* win = dynamic_cast<const MWindow*>(object)) {
            return const_cast<MWindow*>(win);
        }
        object = object->parent();
    }
    return nullptr;
}

} // namespace mulu
