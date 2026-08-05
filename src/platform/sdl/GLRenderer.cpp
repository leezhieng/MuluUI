#include "GLRenderer.h"

#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>

#include "GLFuncs.h"

namespace mulu {

// ---------------------------------------------------------------------------
// Embedded GLSL shaders (OpenGL 3.3 Core, #version 330 core)
// ---------------------------------------------------------------------------

// --- Rounded-rectangle vertex shader ----------------------------------------
static const char* kRectVertexShader = R"glsl(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uProj;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

// --- Rounded-rectangle fragment shader (SDF) --------------------------------
static const char* kRectFragmentShader = R"glsl(#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

uniform vec4 uRectParams;  // (rectWidth, rectHeight, cornerRadius, borderWidth)
uniform vec4 uFillColor;   // (r, g, b, a)

out vec4 fragColor;

float roundedBoxSDF(vec2 p, vec2 halfSize, float r) {
    vec2 d = abs(p) - halfSize + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

void main() {
    float w      = uRectParams.x;
    float h      = uRectParams.y;
    float radius = uRectParams.z;

    vec2 halfSize = vec2(w, h) * 0.5;
    vec2 local    = vTexCoord - halfSize;

    float dist = roundedBoxSDF(local, halfSize, radius);
    float alpha = 1.0 - smoothstep(-1.0, 1.0, dist);

    fragColor = vec4(uFillColor.rgb, uFillColor.a * alpha);
}
)glsl";

// --- Textured-quad vertex shader --------------------------------------------
static const char* kTexVertexShader = R"glsl(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uProj;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

// --- Textured-quad fragment shader (grayscale glyph rendering) --------------
static const char* kTexFragmentShader = R"glsl(#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main() {
    // Direct grayscale alpha — glyph atlas stores coverage values (0=transparent,
    // 255=opaque) produced by stbtt_MakeGlyphBitmap, not signed distance fields.
    float alpha = texture(uTexture, vTexCoord).r;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)glsl";

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

GLRenderer::GLRenderer() = default;

GLRenderer::~GLRenderer()
{
    if (m_programRect) { gl::DeleteProgram(m_programRect); }
    if (m_programTex)  { gl::DeleteProgram(m_programTex); }
    if (m_vao)         { gl::DeleteVertexArrays(1, &m_vao); }
    if (m_vbo)         { gl::DeleteBuffers(1, &m_vbo); }
    if (m_ibo)         { gl::DeleteBuffers(1, &m_ibo); }
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool GLRenderer::initialize()
{
    m_programRect = createProgram(kRectVertexShader, kRectFragmentShader);
    if (!m_programRect) return false;

    m_programTex = createProgram(kTexVertexShader, kTexFragmentShader);
    if (!m_programTex) return false;

    m_uProjRect = gl::GetUniformLocation(m_programRect, "uProj");
    m_uProjTex  = gl::GetUniformLocation(m_programTex,  "uProj");

    // Cache rect-shader uniform locations.
    m_uRectParams = gl::GetUniformLocation(m_programRect, "uRectParams");
    m_uFillColor  = gl::GetUniformLocation(m_programRect, "uFillColor");

    gl::GenVertexArrays(1, &m_vao);
    gl::GenBuffers(1, &m_vbo);
    gl::GenBuffers(1, &m_ibo);

    gl::BindVertexArray(m_vao);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(kMaxVertices * sizeof(Vertex)),
                   nullptr, GL_DYNAMIC_DRAW);

    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, x)));
    gl::EnableVertexAttribArray(0);

    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, u)));
    gl::EnableVertexAttribArray(1);

    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, r)));
    gl::EnableVertexAttribArray(2);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(kMaxIndices * sizeof(uint32_t)),
                   nullptr, GL_DYNAMIC_DRAW);

    gl::BindVertexArray(0);

    m_vertices.reserve(kMaxVertices);
    m_indices.reserve(kMaxIndices);

    m_initialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// Per-frame lifecycle
// ---------------------------------------------------------------------------

void GLRenderer::beginFrame()
{
    m_vertices.clear();
    m_indices.clear();
    m_currentBatch = Batch{};
}

void GLRenderer::endFrame()
{
    flush();
}

// ---------------------------------------------------------------------------
// Shader compilation helpers
// ---------------------------------------------------------------------------

uint32_t GLRenderer::compileShader(uint32_t type, const char* source)
{
    uint32_t shader = gl::CreateShader(type);
    gl::ShaderSource(shader, 1, &source, nullptr);
    gl::CompileShader(shader);

    GLint success = 0;
    gl::GetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        gl::GetShaderInfoLog(shader, sizeof(log), nullptr, log);
        SDL_Log("GLRenderer: shader compilation failed (%s):\n%s",
                (type == GL_VERTEX_SHADER) ? "vertex" : "fragment", log);
        gl::DeleteShader(shader);
        return 0;
    }
    return shader;
}

uint32_t GLRenderer::createProgram(const char* vertSrc, const char* fragSrc)
{
    uint32_t vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    uint32_t fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) {
        if (vs) gl::DeleteShader(vs);
        if (fs) gl::DeleteShader(fs);
        return 0;
    }

    uint32_t program = gl::CreateProgram();
    gl::AttachShader(program, vs);
    gl::AttachShader(program, fs);
    gl::LinkProgram(program);

    GLint success = 0;
    gl::GetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        gl::GetProgramInfoLog(program, sizeof(log), nullptr, log);
        SDL_Log("GLRenderer: program linking failed:\n%s", log);
        gl::DeleteProgram(program);
        program = 0;
    }

    gl::DeleteShader(vs);
    gl::DeleteShader(fs);
    return program;
}

// ---------------------------------------------------------------------------
// Draw commands
// ---------------------------------------------------------------------------

void GLRenderer::drawRoundedRect(float x, float y, float w, float h,
                                  float radius,
                                  float r, float g, float b, float a)
{
    // Flush if we need to switch shaders.
    if (m_currentBatch.programID != 0 &&
        m_currentBatch.programID != m_programRect) {
        flush();
    }

    // Set the rect shader uniforms *before* emitting vertices so flush()
    // sees the correct state.
    gl::UseProgram(m_programRect);
    gl::Uniform4f(m_uRectParams, w, h, radius, 0.0f);
    gl::Uniform4f(m_uFillColor,  r, g, b, a);

    m_currentBatch.programID = m_programRect;
    m_currentBatch.textureID = 0;

    const uint32_t baseVertex = static_cast<uint32_t>(m_vertices.size());
    const float x2 = x + w;
    const float y2 = y + h;

    // vTexCoord encodes the local position inside the rect for the SDF shader.
    m_vertices.push_back({x,  y,  0, 0, r, g, b, a});
    m_vertices.push_back({x2, y,  w, 0, r, g, b, a});
    m_vertices.push_back({x2, y2, w, h, r, g, b, a});
    m_vertices.push_back({x,  y2, 0, h, r, g, b, a});

    m_indices.push_back(baseVertex + 0);
    m_indices.push_back(baseVertex + 1);
    m_indices.push_back(baseVertex + 2);
    m_indices.push_back(baseVertex + 0);
    m_indices.push_back(baseVertex + 2);
    m_indices.push_back(baseVertex + 3);

    m_currentBatch.indexCount += 6;

    // Flush per rect for correct uniform state.  Batching rects with
    // identical uniforms is a future optimization.
    flush();
}

void GLRenderer::drawTexturedQuad(float x, float y, float w, float h,
                                   float u0, float v0, float u1, float v1,
                                   float r, float g, float b, float a)
{
    if (m_currentBatch.programID != 0 &&
        m_currentBatch.programID != m_programTex) {
        flush();
    }
    m_currentBatch.programID = m_programTex;

    const uint32_t baseVertex = static_cast<uint32_t>(m_vertices.size());
    const float x2 = x + w;
    const float y2 = y + h;

    m_vertices.push_back({x,  y,  u0, v0, r, g, b, a});
    m_vertices.push_back({x2, y,  u1, v0, r, g, b, a});
    m_vertices.push_back({x2, y2, u1, v1, r, g, b, a});
    m_vertices.push_back({x,  y2, u0, v1, r, g, b, a});

    m_indices.push_back(baseVertex + 0);
    m_indices.push_back(baseVertex + 1);
    m_indices.push_back(baseVertex + 2);
    m_indices.push_back(baseVertex + 0);
    m_indices.push_back(baseVertex + 2);
    m_indices.push_back(baseVertex + 3);

    m_currentBatch.indexCount += 6;
}

// ---------------------------------------------------------------------------
// Batch flush
// ---------------------------------------------------------------------------

void GLRenderer::flush()
{
    if (m_currentBatch.indexCount == 0 || m_vertices.empty()) {
        m_currentBatch = Batch{};
        return;
    }

    gl::BindVertexArray(m_vao);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferSubData(GL_ARRAY_BUFFER, 0,
                      static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                      m_vertices.data());

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                      static_cast<GLsizeiptr>(m_indices.size() * sizeof(uint32_t)),
                      m_indices.data());

    // Orthographic projection: pixel coords → NDC (Y-down).
    const float L = 0.0f;
    const float R = m_screenW;
    const float T = 0.0f;
    const float B = m_screenH;

    const float proj[16] = {
        2.0f / (R - L),  0.0f,            0.0f,  0.0f,
        0.0f,            2.0f / (T - B),  0.0f,  0.0f,
        0.0f,            0.0f,           -1.0f,  0.0f,
        -(R + L) / (R - L), -(T + B) / (T - B), 0.0f, 1.0f,
    };

    gl::UseProgram(m_currentBatch.programID);

    if (m_currentBatch.programID == m_programRect) {
        gl::UniformMatrix4fv(m_uProjRect, 1, GL_FALSE, proj);
    } else if (m_currentBatch.programID == m_programTex) {
        gl::UniformMatrix4fv(m_uProjTex, 1, GL_FALSE, proj);
    }

    gl::Enable(GL_BLEND);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gl::DrawElements(GL_TRIANGLES,
                     static_cast<GLsizei>(m_currentBatch.indexCount),
                     GL_UNSIGNED_INT,
                     reinterpret_cast<void*>(static_cast<uintptr_t>(
                         m_currentBatch.indexOffset * sizeof(uint32_t))));

    gl::BindVertexArray(0);

    m_currentBatch = Batch{};  // full reset for next batch

    m_vertices.clear();
    m_indices.clear();
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

void GLRenderer::setProjection(float screenW, float screenH)
{
    m_screenW = screenW;
    m_screenH = screenH;
}

} // namespace mulu
