#pragma once

#include <cstdint>

namespace mulu {

// An RGBA color with 8-bit channels.
class MColor {
public:
    MColor() = default;
    MColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
        : m_r(r), m_g(g), m_b(b), m_a(a) {}

    std::uint8_t red() const { return m_r; }
    std::uint8_t green() const { return m_g; }
    std::uint8_t blue() const { return m_b; }
    std::uint8_t alpha() const { return m_a; }

    void setRed(std::uint8_t r) { m_r = r; }
    void setGreen(std::uint8_t g) { m_g = g; }
    void setBlue(std::uint8_t b) { m_b = b; }
    void setAlpha(std::uint8_t a) { m_a = a; }

    bool operator==(const MColor& other) const {
        return m_r == other.m_r && m_g == other.m_g &&
               m_b == other.m_b && m_a == other.m_a;
    }
    bool operator!=(const MColor& other) const { return !(*this == other); }

    // Common named colors.
    static MColor redColor()        { return MColor(255, 0, 0); }
    static MColor greenColor()      { return MColor(0, 255, 0); }
    static MColor blueColor()       { return MColor(0, 0, 255); }
    static MColor whiteColor()      { return MColor(255, 255, 255); }
    static MColor blackColor()      { return MColor(0, 0, 0); }
    static MColor grayColor()       { return MColor(128, 128, 128); }
    static MColor transparentColor() { return MColor(0, 0, 0, 0); }

private:
    std::uint8_t m_r = 0;
    std::uint8_t m_g = 0;
    std::uint8_t m_b = 0;
    std::uint8_t m_a = 255;
};

} // namespace mulu
