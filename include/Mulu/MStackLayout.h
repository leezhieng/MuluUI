#pragma once

#include <vector>

#include "Mulu/MWidget.h"

namespace mulu {

// Arranges child widgets in a row or a column (maps to a native StackPanel).
class MStackLayout : public MWidget {
public:
    explicit MStackLayout(MWidget* parent = nullptr);

    enum class Orientation { Vertical, Horizontal };

    void setOrientation(Orientation orientation);
    Orientation orientation() const { return m_orientation; }

    void addWidget(MWidget* widget);
    void removeWidget(MWidget* widget);
    void clear();

    const std::vector<MWidget*>& widgets() const { return m_widgets; }

    // layout() override — distributes space among children.
    void layout() override;

private:
    Orientation m_orientation = Orientation::Vertical;
    std::vector<MWidget*> m_widgets;
};

} // namespace mulu
