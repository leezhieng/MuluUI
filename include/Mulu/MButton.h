#pragma once

#include <functional>

#include "Mulu/MString.h"
#include "Mulu/MWidget.h"

namespace mulu {

// A push button (maps to a native Button).
class MButton : public MWidget {
public:
    explicit MButton(MWidget* parent = nullptr);
    explicit MButton(const MString& text, MWidget* parent = nullptr);

    void setText(const MString& text);
    MString text() const { return m_text; }

    using ClickCallback = std::function<void()>;
    void setOnClicked(ClickCallback callback) { m_onClicked = std::move(callback); }

    // Invoked by the platform backend when the native button is pressed.
    void click();

private:
    MString m_text;
    ClickCallback m_onClicked;
};

} // namespace mulu
