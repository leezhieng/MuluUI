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

void MStackLayout::layout()
{
    const int count = static_cast<int>(m_widgets.size());
    if (count == 0) {
        return;
    }

    const int lx = m_geometry.x();
    const int ly = m_geometry.y();
    const int lw = m_geometry.width();
    const int lh = m_geometry.height();

    if (m_orientation == Orientation::Vertical) {
        const int childHeight = lh / count;
        int y = ly;
        for (int i = 0; i < count; ++i) {
            MWidget* child = m_widgets[i];
            // Auto-size children that haven't been given an explicit
            // geometry yet (width == 0 || height == 0 → uninitialised).
            if (child->geometry().isEmpty()) {
                child->setGeometry(MRect(lx, y, lw, childHeight));
            }
            // Advance by the child's actual height (explicit or computed).
            y += child->geometry().height();
            child->layout(); // recurse into nested layouts
        }
    } else {
        const int childWidth = lw / count;
        int x = lx;
        for (int i = 0; i < count; ++i) {
            MWidget* child = m_widgets[i];
            if (child->geometry().isEmpty()) {
                child->setGeometry(MRect(x, ly, childWidth, lh));
            }
            // Advance by the child's actual width (explicit or computed).
            x += child->geometry().width();
            child->layout(); // recurse into nested layouts
        }
    }
}

} // namespace mulu
