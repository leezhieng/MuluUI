#include "TextRenderer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>

// ---------------------------------------------------------------------------
// stb_truetype – single-header TrueType font rasteriser (public domain).
// Downloaded by download_dep.py into dependencies/install/include/.
// ---------------------------------------------------------------------------
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "GLFuncs.h"
#include "GLRenderer.h"

namespace mulu {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer()
{
    if (m_atlasTexture) {
        gl::DeleteTextures(1, &m_atlasTexture);
    }
    if (m_fontInfo) {
        delete m_fontInfo;
        m_fontInfo = nullptr;
    }
    free(m_fontData);
    m_fontData = nullptr;
}

// ---------------------------------------------------------------------------
// Font loading
// ---------------------------------------------------------------------------

bool TextRenderer::loadFont(const char* ttfPath, float fontSize)
{
    // Read the entire .ttf file into memory.
    FILE* f = fopen(ttfPath, "rb");
    if (!f) {
        SDL_Log("TextRenderer: failed to open font file: %s", ttfPath);
        return false;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    m_fontData = static_cast<unsigned char*>(malloc(static_cast<size_t>(size)));
    if (!m_fontData) {
        fclose(f);
        return false;
    }
    fread(m_fontData, 1, static_cast<size_t>(size), f);
    fclose(f);

    // Initialise stb_truetype.
    m_fontInfo = new stbtt_fontinfo();
    const int offset = stbtt_GetFontOffsetForIndex(m_fontData, 0);
    if (!stbtt_InitFont(m_fontInfo, m_fontData, offset)) {
        SDL_Log("TextRenderer: stbtt_InitFont failed");
        return false;
    }

    m_fontSize = fontSize;
    m_scale = stbtt_ScaleForMappingEmToPixels(m_fontInfo, fontSize);

    stbtt_GetFontVMetrics(m_fontInfo, &m_ascent, &m_descent, &m_lineGap);
    // Convert from font units to pixels.
    m_ascent  = static_cast<int>(m_ascent  * m_scale);
    m_descent = static_cast<int>(m_descent * m_scale);
    m_lineGap = static_cast<int>(m_lineGap * m_scale);

    SDL_Log("TextRenderer: font loaded (%s, %.0fpx, ascent=%d)", ttfPath,
            static_cast<double>(fontSize), m_ascent);
    return createAtlas();
}

bool TextRenderer::loadFontFromMemory(const unsigned char* data,
                                       unsigned int dataSize, float fontSize)
{
    if (!data || dataSize == 0) {
        SDL_Log("TextRenderer: loadFontFromMemory received null/empty data");
        return false;
    }

    // Free any previously loaded font.
    free(m_fontData);
    m_fontData = nullptr;
    if (m_fontInfo) {
        delete m_fontInfo;
        m_fontInfo = nullptr;
    }
    m_glyphCache.clear();

    // Copy the font data so we own it.
    m_fontData = static_cast<unsigned char*>(malloc(dataSize));
    if (!m_fontData) {
        return false;
    }
    memcpy(m_fontData, data, dataSize);

    // Diagnostic: print first 8 bytes of font data.
    SDL_Log("TextRenderer: font data size=%u, first bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
            dataSize,
            m_fontData[0], m_fontData[1], m_fontData[2], m_fontData[3],
            m_fontData[4], m_fontData[5], m_fontData[6], m_fontData[7]);

    m_fontInfo = new stbtt_fontinfo();
    const int offset = stbtt_GetFontOffsetForIndex(m_fontData, 0);
    SDL_Log("TextRenderer: font offset for index 0 = %d", offset);
    if (!stbtt_InitFont(m_fontInfo, m_fontData, offset)) {
        SDL_Log("TextRenderer: stbtt_InitFont failed (from memory)");
        return false;
    }
    SDL_Log("TextRenderer: stbtt_InitFont succeeded");

    m_fontSize = fontSize;
    m_scale = stbtt_ScaleForMappingEmToPixels(m_fontInfo, fontSize);

    stbtt_GetFontVMetrics(m_fontInfo, &m_ascent, &m_descent, &m_lineGap);
    m_ascent  = static_cast<int>(m_ascent  * m_scale);
    m_descent = static_cast<int>(m_descent * m_scale);
    m_lineGap = static_cast<int>(m_lineGap * m_scale);

    SDL_Log("TextRenderer: font loaded from memory (%.0fpx, ascent=%d)",
            static_cast<double>(fontSize), m_ascent);
    return createAtlas();
}

bool TextRenderer::createAtlas()
{
    gl::GenTextures(1, &m_atlasTexture);
    gl::BindTexture(GL_TEXTURE_2D, m_atlasTexture);

    // Single-channel (R8) texture for glyph atlas.
    // Initialise to zeros so un-rasterised regions are transparent,
    // not random GPU memory.
    {
        std::vector<unsigned char> zeros(
            static_cast<size_t>(m_atlasWidth * m_atlasHeight), 0);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                       m_atlasWidth, m_atlasHeight, 0,
                       GL_RED, GL_UNSIGNED_BYTE, zeros.data());
    }

    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::BindTexture(GL_TEXTURE_2D, 0);

    m_atlasCursorX   = 1;
    m_atlasCursorY   = 1;
    m_atlasRowHeight = 0;

    SDL_Log("TextRenderer: atlas created (%dx%d R8)", m_atlasWidth, m_atlasHeight);
    return true;
}

// ---------------------------------------------------------------------------
// Glyph rasterisation
// ---------------------------------------------------------------------------

void TextRenderer::rasteriseGlyph(uint32_t codepoint, GlyphInfo& info)
{
    const int glyphIndex = stbtt_FindGlyphIndex(m_fontInfo, static_cast<int>(codepoint));

    static int s_glyphCount = 0;
    if (s_glyphCount < 5) {
        SDL_Log("TextRenderer: rasteriseGlyph U+%04X glyphIdx=%d", codepoint, glyphIndex);
        ++s_glyphCount;
    }

    int advanceWidth = 0, leftSideBearing = 0;
    stbtt_GetGlyphHMetrics(m_fontInfo, glyphIndex, &advanceWidth, &leftSideBearing);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(m_fontInfo, glyphIndex, m_scale, m_scale, &x0, &y0, &x1, &y1);

    const int gw = x1 - x0;
    const int gh = y1 - y0;

    info.ax = advanceWidth * m_scale;
    info.ay = 0.0f;
    info.bx = static_cast<float>(x0);
    info.by = static_cast<float>(y0);
    info.bw = static_cast<float>(gw);
    info.bh = static_cast<float>(gh);

    if (gw <= 0 || gh <= 0) {
        info.tx = 0.0f; info.ty = 0.0f; info.tw = 0.0f; info.th = 0.0f;
        return;
    }

    // Allocate space in the atlas.
    if (m_atlasCursorX + gw + 2 > m_atlasWidth) {
        m_atlasCursorX = 1;
        m_atlasCursorY += m_atlasRowHeight + 2;
        m_atlasRowHeight = 0;
    }
    if (m_atlasCursorY + gh + 2 > m_atlasHeight) {
        SDL_Log("TextRenderer: glyph atlas overflow at U+%04X", codepoint);
        info.tx = 0.0f; info.ty = 0.0f; info.tw = 0.0f; info.th = 0.0f;
        return;
    }

    // Rasterise standard grayscale glyph bitmap.
    std::vector<unsigned char> bitmap(static_cast<size_t>(gw * gh), 0);
    stbtt_MakeGlyphBitmap(m_fontInfo, bitmap.data(), gw, gh, gw,
                          m_scale, m_scale, glyphIndex);

    // Diagnostic: count non-zero bytes in the bitmap.
    {
        size_t nonZero = 0;
        for (unsigned char b : bitmap) { if (b != 0) ++nonZero; }
        static int s_bmpLogCount = 0;
        if (s_bmpLogCount < 3) {
            SDL_Log("TextRenderer: glyph U+%04X %dx%d, nonZero=%zu/%zu",
                    codepoint, gw, gh, nonZero, bitmap.size());
            ++s_bmpLogCount;
        }
    }

    gl::BindTexture(GL_TEXTURE_2D, m_atlasTexture);
    gl::TexSubImage2D(GL_TEXTURE_2D, 0,
                      m_atlasCursorX, m_atlasCursorY, gw, gh,
                      GL_RED, GL_UNSIGNED_BYTE, bitmap.data());

    info.tx = static_cast<float>(m_atlasCursorX) / static_cast<float>(m_atlasWidth);
    info.ty = static_cast<float>(m_atlasCursorY) / static_cast<float>(m_atlasHeight);
    info.tw = static_cast<float>(gw)             / static_cast<float>(m_atlasWidth);
    info.th = static_cast<float>(gh)             / static_cast<float>(m_atlasHeight);

    m_atlasCursorX += gw + 2;
    if (gh > m_atlasRowHeight) {
        m_atlasRowHeight = gh;
    }
}

// ---------------------------------------------------------------------------
// Text drawing
// ---------------------------------------------------------------------------

void TextRenderer::drawText(GLRenderer& renderer, const MString& text,
                             float x, float y,
                             float r, float g, float b, float a)
{
    if (!m_fontInfo || text.isEmpty()) {
        return;
    }

    // Bind the glyph atlas so textured quads use it.
    gl::ActiveTexture(GL_TEXTURE0);
    gl::BindTexture(GL_TEXTURE_2D, m_atlasTexture);
    // The textured-quad shader samples from texture unit 0 by default.

    const std::string utf8 = text.toStdString();
    float penX = x;
    const float baselineY = y + static_cast<float>(m_ascent);

    for (size_t i = 0; i < utf8.size(); ) {
        // Decode one UTF-8 codepoint.
        uint32_t cp = 0;
        int      len = 0;

        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        if ((c & 0x80) == 0) {
            cp = c;
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
        } else {
            ++i;
            continue; // invalid UTF-8
        }
        for (int j = 1; j < len && (i + j) < utf8.size(); ++j) {
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + j]) & 0x3F);
        }
        i += static_cast<size_t>(len);

        // Look up glyph in cache or rasterise on demand.
        auto it = m_glyphCache.find(cp);
        if (it == m_glyphCache.end()) {
            GlyphInfo info{};
            rasteriseGlyph(cp, info);
            it = m_glyphCache.emplace(cp, info).first;
        }

        const GlyphInfo& glyph = it->second;

        if (glyph.bw > 0.0f && glyph.bh > 0.0f) {
            const float gx = penX + glyph.bx;
            const float gy = baselineY + glyph.by;
            renderer.drawTexturedQuad(gx, gy, glyph.bw, glyph.bh,
                                       glyph.tx, glyph.ty,
                                       glyph.tx + glyph.tw, glyph.ty + glyph.th,
                                       r, g, b, a);
        }

        penX += glyph.ax;
    }
}

// ---------------------------------------------------------------------------
// Text measurement
// ---------------------------------------------------------------------------

float TextRenderer::measureText(const MString& text) const
{
    if (!m_fontInfo || text.isEmpty()) {
        return 0.0f;
    }

    const std::string utf8 = text.toStdString();
    float width = 0.0f;

    for (size_t i = 0; i < utf8.size(); ) {
        uint32_t cp = 0;
        int      len = 0;
        const unsigned char c = static_cast<unsigned char>(utf8[i]);

        if ((c & 0x80) == 0)          { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0)  { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0)  { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0)  { cp = c & 0x07; len = 4; }
        else { ++i; continue; }

        for (int j = 1; j < len && (i + j) < utf8.size(); ++j) {
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + j]) & 0x3F);
        }
        i += static_cast<size_t>(len);

        auto it = m_glyphCache.find(cp);
        if (it != m_glyphCache.end()) {
            width += it->second.ax;
        } else {
            // Estimate for uncached glyphs.
            width += m_fontSize * 0.5f;
        }
    }
    return width;
}

float TextRenderer::lineHeight() const
{
    return static_cast<float>(m_ascent - m_descent);
}

} // namespace mulu
