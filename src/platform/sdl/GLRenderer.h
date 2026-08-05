#pragma once

#include <cstdint>
#include <vector>

namespace mulu {

// ---------------------------------------------------------------------------
// GLRenderer – Batched 2D GPU renderer for the SDL3 + OpenGL 3.3 backend.
//
// Collects draw commands into a vertex buffer and flushes them in a single
// (or few) draw calls per frame.  Supports:
//   • Solid rounded rectangles (Fluent Design buttons, panels)
//   • Textured glyph quads (emitted by TextRenderer)
//
// Usage per frame:
//   renderer.beginFrame();
//   renderer.drawRoundedRect(...);
//   textRenderer.drawText(renderer, ...);   // pushes textured quads
//   renderer.endFrame();
// ---------------------------------------------------------------------------
class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();

    GLRenderer(const GLRenderer&) = delete;
    GLRenderer& operator=(const GLRenderer&) = delete;

    // One-time initialization after an OpenGL 3.3 Core context is current.
    bool initialize();

    // Per-frame lifecycle.
    void beginFrame();
    void endFrame();

    // --- Draw commands ------------------------------------------------------

    // Push a solid rounded rectangle into the batch.
    // (x, y) = top-left in pixel coordinates; (w, h) = size; radius in px.
    void drawRoundedRect(float x, float y, float w, float h,
                         float radius,
                         float r, float g, float b, float a);

    // Push a textured quad into the batch (used by TextRenderer for glyphs).
    // textureID must already be bound by the caller.
    void drawTexturedQuad(float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1,
                          float r, float g, float b, float a);

    // --- Shader uniform setters (called between begin/end) ------------------

    void setProjection(float screenW, float screenH);

private:
    // --- Internal vertex format ---------------------------------------------

    struct Vertex {
        float x, y;         // position (pixels)
        float u, v;         // texture coordinates (for text glyphs)
        float r, g, b, a;   // color
    };

    // --- Batch state --------------------------------------------------------

    struct Batch {
        uint32_t     textureID = 0;     // 0 = solid color (no texture)
        uint32_t     programID = 0;     // shader program currently in use
        uint32_t     indexOffset = 0;   // first index in this batch
        uint32_t     indexCount  = 0;   // number of indices to draw
    };

    void flush();                        // upload & draw current batch, then reset
    uint32_t compileShader(uint32_t type, const char* source);
    uint32_t createProgram(const char* vertSrc, const char* fragSrc);

    // --- GPU resources ------------------------------------------------------

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_ibo = 0;

    uint32_t m_programRect  = 0;  // rounded-rect shader
    uint32_t m_programTex   = 0;  // textured quad shader

    int m_uProjRect   = -1;       // uniform location: projection matrix (rect)
    int m_uProjTex    = -1;       // uniform location: projection matrix (tex)
    int m_uRectParams = -1;       // uniform location: uRectParams (rect shader)
    int m_uFillColor  = -1;       // uniform location: uFillColor (rect shader)

    // --- Batch accumulation -------------------------------------------------

    static constexpr size_t kMaxVertices = 65536;
    static constexpr size_t kMaxIndices  = 98304; // 65536 quads * 6/4

    std::vector<Vertex>  m_vertices;
    std::vector<uint32_t> m_indices;
    Batch                m_currentBatch;

    float m_screenW = 800.0f;
    float m_screenH = 600.0f;
    bool  m_initialized = false;
};

} // namespace mulu
