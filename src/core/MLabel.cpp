#include "Mulu/MLabel.h"

#include "Mulu/MWindow.h"

namespace mulu {

MLabel::MLabel(MWidget* parent)
    : MWidget(parent)
{
}

MLabel::MLabel(const MString& text, MWidget* parent)
    : MWidget(parent),
      m_text(text)
{
}

void MLabel::setText(const MString& text)
{
    if (m_text == text) {
        return;
    }
    m_text = text;
    if (MWindow* win = window()) {
        win->requestWidgetSync(this);
    }
}

} // namespace mulu
