#pragma once

namespace mulu {

// A 2D size in integer pixels.
class MSize {
public:
    MSize() = default;
    MSize(int width, int height) : m_width(width), m_height(height) {}

    int width() const { return m_width; }
    int height() const { return m_height; }

    void setWidth(int width) { m_width = width; }
    void setHeight(int height) { m_height = height; }
    void setSize(int width, int height) {
        m_width = width;
        m_height = height;
    }

    bool isEmpty() const { return m_width <= 0 || m_height <= 0; }
    bool isNull() const { return m_width == 0 && m_height == 0; }

    bool operator==(const MSize& other) const {
        return m_width == other.m_width && m_height == other.m_height;
    }
    bool operator!=(const MSize& other) const { return !(*this == other); }

private:
    int m_width = 0;
    int m_height = 0;
};

} // namespace mulu
