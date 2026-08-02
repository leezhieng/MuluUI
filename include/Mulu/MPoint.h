#pragma once

namespace mulu {

// A point in 2D integer space.
class MPoint {
public:
    MPoint() = default;
    MPoint(int x, int y) : m_x(x), m_y(y) {}

    int x() const { return m_x; }
    int y() const { return m_y; }

    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }

    bool isNull() const { return m_x == 0 && m_y == 0; }

    bool operator==(const MPoint& other) const {
        return m_x == other.m_x && m_y == other.m_y;
    }
    bool operator!=(const MPoint& other) const { return !(*this == other); }

private:
    int m_x = 0;
    int m_y = 0;
};

} // namespace mulu
