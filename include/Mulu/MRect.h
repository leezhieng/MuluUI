#pragma once

#include "Mulu/MPoint.h"
#include "Mulu/MSize.h"

namespace mulu {

// An axis-aligned rectangle in integer coordinates.
class MRect {
public:
    MRect() = default;
    MRect(int x, int y, int width, int height)
        : m_x(x), m_y(y), m_width(width), m_height(height) {}
    MRect(const MPoint& topLeft, const MSize& size)
        : m_x(topLeft.x()), m_y(topLeft.y()),
          m_width(size.width()), m_height(size.height()) {}

    int x() const { return m_x; }
    int y() const { return m_y; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }
    void setWidth(int width) { m_width = width; }
    void setHeight(int height) { m_height = height; }
    void setRect(int x, int y, int width, int height) {
        m_x = x;
        m_y = y;
        m_width = width;
        m_height = height;
    }

    MPoint topLeft() const { return MPoint(m_x, m_y); }
    MSize size() const { return MSize(m_width, m_height); }

    void setPosition(const MPoint& pos) {
        m_x = pos.x();
        m_y = pos.y();
    }
    void setSize(const MSize& size) {
        m_width = size.width();
        m_height = size.height();
    }

    bool isEmpty() const { return m_width <= 0 || m_height <= 0; }
    bool contains(const MPoint& p) const {
        return p.x() >= m_x && p.x() < m_x + m_width &&
               p.y() >= m_y && p.y() < m_y + m_height;
    }

    bool operator==(const MRect& other) const {
        return m_x == other.m_x && m_y == other.m_y &&
               m_width == other.m_width && m_height == other.m_height;
    }
    bool operator!=(const MRect& other) const { return !(*this == other); }

private:
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace mulu
