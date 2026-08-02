#include "Mulu/MButton.h"

#include "Mulu/MWindow.h"

namespace mulu {

MButton::MButton(MWidget* parent)
    : MWidget(parent)
{
}

MButton::MButton(const MString& text, MWidget* parent)
    : MWidget(parent),
      m_text(text)
{
}

void MButton::setText(const MString& text)
{
    if (m_text == text) {
        return;
    }
    m_text = text;
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

void MButton::click()
{
    if (m_onClicked) {
        m_onClicked();
    }
}

} // namespace mulu
