#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Mulu/MString.h"

// Forward-declare stbtt types we use.
// stb_truetype.h must be included with STB_TRUETYPE_IMPLEMENTATION in exactly
// one translation unit (TextRenderer.cpp).  See:
//   https://github.com/nothings/stb/blob/master/stb_truetype.h
struct stbtt_fontinfo;

namespace mulu {

class GLRenderer;

// ---------------------------------------------------------------------------
// TextRenderer – SDF glyph-atlas text rendering for the SDL3+OpenGL backend.
//
// Uses stb_truetype to rasterise glyphs into a single-channel (R8) signed-
// distance-field (SDF) atlas texture.  Glyphs are cached on first use; drawText
// emits a series of textured quads into the GLRenderer batch.
//
// Dependencies:
//   stb_truetype.h  – https://github.com/nothings/stb (public domain)
//   A .ttf font file – Segoe UI Variable (Windows) or any TrueType font.
// ---------------------------------------------------------------------------
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    // Load a TrueType font from a file path.  Must be called before drawText.
    // Returns false if the font could not be loaded or the atlas could not be
    // created.
    bool loadFont(const char* ttfPath, float fontSize = 32.0f);

    // Load a TrueType font from an in-memory buffer (e.g. embedded resource).
    // The data pointer must remain valid for the lifetime of the TextRenderer.
    bool loadFontFromMemory(const unsigned char* data, unsigned int dataSize,
                            float fontSize = 32.0f);

    // Draw a string at pixel position (x, y) with the given RGBA color.
    // Pushes textured glyph quads into the renderer's batch; the caller is
    // responsible for calling GLRenderer::beginFrame/endFrame around this.
    void drawText(GLRenderer& renderer, const MString& text,
                  float x, float y,
                  float r, float g, float b, float a);

    // Return the pixel width of a text string (for centering / layout).
    float measureText(const MString& text) const;

    // Return the line height (ascent - descent) in pixels.
    float lineHeight() const;

private:
    // --- Glyph metrics (in pixels at the loaded font size) ------------------

    struct GlyphInfo {
        float ax;      // advance x
        float ay;      // advance y
        float bx;      // bearing x (offset from origin to left edge)
        float by;      // bearing y (offset from baseline to top edge)
        float bw;      // bitmap width
        float bh;      // bitmap height
        float tx;      // texture x (normalised 0–1)
        float ty;      // texture y
        float tw;      // texture width  (normalised)
        float th;      // texture height (normalised)
    };

    // --- Atlas texture ------------------------------------------------------

    bool createAtlas();
    void rasteriseGlyph(uint32_t codepoint, GlyphInfo& info);

    // --- Data members -------------------------------------------------------

    stbtt_fontinfo* m_fontInfo = nullptr;
    unsigned char*  m_fontData = nullptr;
    float           m_fontSize = 32.0f;
    float           m_scale    = 1.0f;     // stbtt_ScaleForMappingEmToPixels
    int             m_ascent   = 0;
    int             m_descent  = 0;
    int             m_lineGap  = 0;

    uint32_t m_atlasTexture   = 0;
    int      m_atlasWidth     = 1024;
    int      m_atlasHeight    = 1024;
    int      m_atlasCursorX   = 1;         // 1-pixel padding
    int      m_atlasCursorY   = 1;
    int      m_atlasRowHeight = 0;

    // On-demand glyph cache.
    std::unordered_map<uint32_t, GlyphInfo> m_glyphCache;
};

} // namespace mulu
