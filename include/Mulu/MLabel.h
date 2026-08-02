#pragma once

#include "Mulu/MString.h"
#include "Mulu/MWidget.h"

namespace mulu {

// A static text label (maps to a native TextBlock/TextView).
class MLabel : public MWidget {
public:
    explicit MLabel(MWidget* parent = nullptr);
    explicit MLabel(const MString& text, MWidget* parent = nullptr);

    void setText(const MString& text);
    MString text() const { return m_text; }

private:
    MString m_text;
};

} // namespace mulu
