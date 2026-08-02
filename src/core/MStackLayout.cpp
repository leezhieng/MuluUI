#include "Mulu/MStackLayout.h"

#include <algorithm>

#include "Mulu/MWindow.h"

namespace mulu {

MStackLayout::MStackLayout(MWidget* parent)
    : MWidget(parent)
{
}

void MStackLayout::setOrientation(Orientation orientation)
{
    if (m_orientation == orientation) {
        return;
    }
    m_orientation = orientation;
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MStackLayout::addWidget(MWidget* widget)
{
    if (!widget) {
        return;
    }
    if (std::find(m_widgets.begin(), m_widgets.end(), widget) != m_widgets.end()) {
        return;
    }
    m_widgets.push_back(widget);
    widget->setParent(this);
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MStackLayout::removeWidget(MWidget* widget)
{
    const auto it = std::find(m_widgets.begin(), m_widgets.end(), widget);
    if (it == m_widgets.end()) {
        return;
    }
    m_widgets.erase(it);
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MStackLayout::clear()
{
    m_widgets.clear();
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

} // namespace mulu
