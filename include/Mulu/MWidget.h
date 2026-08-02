#pragma once

#include "Mulu/MObject.h"
#include "Mulu/MRect.h"

namespace mulu {

class MWindow;

// Base class for all user-interface elements (QWidget equivalent).
class MWidget : public MObject {
public:
    explicit MWidget(MWidget* parent = nullptr);
    ~MWidget() override;

    // Geometry --------------------------------------------------------------
    void setGeometry(const MRect& geometry);
    MRect geometry() const { return m_geometry; }

    void setPosition(const MPoint& position);
    MPoint position() const { return m_geometry.topLeft(); }

    void setSize(const MSize& size);
    MSize size() const { return m_geometry.size(); }

    // Visibility ------------------------------------------------------------
    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }
    void show();
    void hide();

    // The top-level window this widget lives in (walks the parent chain).
    MWindow* window() const;

    // Backend notification hooks (override to customize native behavior).
    virtual void onShown() {}
    virtual void onHidden() {}

protected:
    MRect m_geometry;
    bool m_visible = false;
};

} // namespace mulu
