#include "MOpenGLWindow.h"

#include <SDL3/SDL.h>

#include "GLFuncs.h"
#include "GLRenderer.h"
#include "MOpenGLPlatform.h"

#ifndef MULU_NO_TEXT_RENDERER
#include "TextRenderer.h"
#endif

#include "Mulu/MButton.h"
#include "Mulu/MLabel.h"
#include "Mulu/MStackLayout.h"
#include "Mulu/MWidget.h"
#include "Mulu/MWindow.h"

namespace mulu {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MOpenGLWindow::MOpenGLWindow(MWindow* owner)
    : m_owner(owner)
{
    if (owner) {
        m_title = owner->title();
        m_size  = owner->size();
    }
}

MOpenGLWindow::~MOpenGLWindow()
{
    // Drop the renderer before the GL context.
    m_renderer.reset();
    m_textRenderer.reset();

    if (m_glContext) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_sdlWindow) {
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
    }
}

// ---------------------------------------------------------------------------
// SDL3 window + OpenGL context creation
// ---------------------------------------------------------------------------

bool MOpenGLWindow::createSDLWindow()
{
    if (m_sdlWindow) {
        return true; // already created
    }

    // Request OpenGL 3.3 Core profile before creating the window.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // RGBA8 framebuffer with alpha for transparency effects (Acrylic).
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    m_sdlWindow = SDL_CreateWindow(
        m_title.toStdString().c_str(),
        static_cast<int>(m_size.width()),
        static_cast<int>(m_size.height()),
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!m_sdlWindow) {
        SDL_Log("MOpenGLWindow: SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    m_sdlWindowID = SDL_GetWindowID(m_sdlWindow);

    // Create the GL context and make it current.
    m_glContext = SDL_GL_CreateContext(m_sdlWindow);
    if (!m_glContext) {
        SDL_Log("MOpenGLWindow: SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
        return false;
    }

    SDL_GL_MakeCurrent(m_sdlWindow, m_glContext);

    // Enable vsync (1 = on, 0 = off, -1 = adaptive).
    SDL_GL_SetSwapInterval(1);

    // Load OpenGL 3.3 Core function pointers.
    if (!gl::loadGLFunctions()) {
        SDL_Log("MOpenGLWindow: failed to load OpenGL functions");
        return false;
    }

    // Create the batch renderer and text renderer now that GL is available.
    m_renderer     = std::make_unique<GLRenderer>();
#ifndef MULU_NO_TEXT_RENDERER
    m_textRenderer = std::make_unique<TextRenderer>();

    // Prefer embedded font data (set via setFontData()), fall back to
    // probing common system font paths.
    bool fontLoaded = false;
    if (m_fontData && m_fontDataSize > 0) {
        fontLoaded = m_textRenderer->loadFontFromMemory(
            m_fontData, m_fontDataSize, 24.0f);
    }
    if (!fontLoaded) {
        static const char* kFontPaths[] = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
        };
        for (const char* path : kFontPaths) {
            if (m_textRenderer->loadFont(path, 24.0f)) {
                fontLoaded = true;
                break;
            }
        }
    }
    if (!fontLoaded) {
        SDL_Log("MOpenGLWindow: no font found — text rendering disabled");
        m_textRenderer.reset();
    }
#endif

    if (!m_renderer->initialize()) {
        SDL_Log("MOpenGLWindow: GLRenderer initialization failed");
        return false;
    }

    m_glInitialized = true;

    SDL_Log("MOpenGLWindow: SDL window + GL 3.3 context created (%dx%d)",
            static_cast<int>(m_size.width()),
            static_cast<int>(m_size.height()));
    return true;
}

// ---------------------------------------------------------------------------
// MPlatformWindow overrides
// ---------------------------------------------------------------------------

void MOpenGLWindow::show()
{
    m_visible = true;

    // Lazily create the SDL window and GL context on first show.
    if (!m_sdlWindow) {
        if (!createSDLWindow()) {
            m_visible = false;
            return;
        }
    } else {
        SDL_ShowWindow(m_sdlWindow);
    }

    // Sync the widget tree to build the initial draw list.
    if (m_owner && m_owner->centralWidget()) {
        syncWidgetTree(m_owner->centralWidget());
    }

    // Request an immediate repaint.
    if (auto* platform = MOpenGLPlatform::instance()) {
        platform->requestRender();
    }
}

void MOpenGLWindow::hide()
{
    m_visible = false;
    if (m_sdlWindow) {
        SDL_HideWindow(m_sdlWindow);
    }
}

void MOpenGLWindow::close()
{
    m_pendingClose = true;
}

void MOpenGLWindow::setTitle(const MString& title)
{
    m_title = title;
    if (m_sdlWindow) {
        SDL_SetWindowTitle(m_sdlWindow, title.toStdString().c_str());
    }
}

void MOpenGLWindow::setSize(const MSize& size)
{
    m_size = size;
    if (m_sdlWindow) {
        SDL_SetWindowSize(m_sdlWindow,
                          static_cast<int>(size.width()),
                          static_cast<int>(size.height()));
    }
}

void* MOpenGLWindow::nativeHandle()
{
    // Return the underlying native window handle (HWND on Windows).
    if (!m_sdlWindow) {
        return nullptr;
    }
    SDL_PropertiesID props = SDL_GetWindowProperties(m_sdlWindow);
    return reinterpret_cast<void*>(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

// ---------------------------------------------------------------------------
// Widget tree → draw list conversion
// ---------------------------------------------------------------------------

void MOpenGLWindow::syncWidgetTree(MWidget* root)
{
    m_drawList.clear();
    if (root) {
        buildDrawListRecursive(root, -1);
    }
}

#ifndef MULU_NO_TEXT_RENDERER
void MOpenGLWindow::setFontData(const unsigned char* data, unsigned int size)
{
    m_fontData     = data;
    m_fontDataSize = size;
}
#endif

void MOpenGLWindow::buildDrawListRecursive(MWidget* widget, int parentIndex)
{
    if (!widget) {
        return;
    }

    DrawNode node;
    node.rect       = widget->geometry();
    node.parentIndex = parentIndex;

    const int myIndex = static_cast<int>(m_drawList.size());

    // Classify the widget by its concrete type.
    if (auto* button = dynamic_cast<MButton*>(widget)) {
        node.type   = DrawNode::Button;
        node.text   = button->text();
        node.button = button;
    } else if (auto* label = dynamic_cast<MLabel*>(widget)) {
        node.type = DrawNode::Label;
        node.text = label->text();
    } else if (auto* layout = dynamic_cast<MStackLayout*>(widget)) {
        node.type = DrawNode::StackLayout;
    } else {
        node.type = DrawNode::None;
    }

    m_drawList.push_back(node);

    // Recurse into children (layouts own child widgets).
    if (auto* layout = dynamic_cast<MStackLayout*>(widget)) {
        for (MWidget* child : layout->widgets()) {
            buildDrawListRecursive(child, myIndex);
        }
    }

    // Also handle plain MWidget children (e.g. centralwidget holding a layout).
    for (MObject* child : widget->children()) {
        if (auto* childWidget = dynamic_cast<MWidget*>(child)) {
            // Skip widgets already handled as layout children above.
            bool alreadyAdded = false;
            if (auto* layout = dynamic_cast<MStackLayout*>(widget)) {
                for (MWidget* lw : layout->widgets()) {
                    if (lw == childWidget) {
                        alreadyAdded = true;
                        break;
                    }
                }
            }
            if (!alreadyAdded) {
                buildDrawListRecursive(childWidget, myIndex);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Hit-testing
// ---------------------------------------------------------------------------

int MOpenGLWindow::hitTest(int x, int y) const
{
    int bestIndex = -1;
    int bestDepth = -1;

    for (int i = 0; i < static_cast<int>(m_drawList.size()); ++i) {
        const DrawNode& node = m_drawList[i];
        if (node.rect.contains(MPoint(x, y))) {
            // Compute depth by walking parent chain.
            int depth = 0;
            int p = node.parentIndex;
            while (p >= 0) {
                ++depth;
                p = m_drawList[p].parentIndex;
            }
            if (depth >= bestDepth) {
                bestDepth = depth;
                bestIndex = i;
            }
        }
    }
    return bestIndex;
}

// ---------------------------------------------------------------------------
// SDL event dispatch
// ---------------------------------------------------------------------------

void MOpenGLWindow::handleSDLEvent(const SDL_Event& event)
{
    switch (event.type) {

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (event.window.windowID == m_sdlWindowID) {
            m_pendingClose = true;
        }
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        if (event.window.windowID == m_sdlWindowID) {
            m_size = MSize(
                static_cast<int>(event.window.data1),
                static_cast<int>(event.window.data2));
            gl::Viewport(0, 0,
                         static_cast<int>(m_size.width()),
                         static_cast<int>(m_size.height()));
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (event.button.windowID != m_sdlWindowID) break;
        if (event.button.button != SDL_BUTTON_LEFT) break;

        const int mx = static_cast<int>(event.button.x);
        const int my = static_cast<int>(event.button.y);

        const int nodeIdx = hitTest(mx, my);
        if (nodeIdx >= 0 && nodeIdx < static_cast<int>(m_drawList.size())) {
            const DrawNode& node = m_drawList[nodeIdx];
            if (node.type == DrawNode::Button && node.button) {
                node.button->click();
            }
        }
        break;
    }

    case SDL_EVENT_MOUSE_MOTION: {
        // Track cursor for hover effects (Reveal highlight) – future enhancement.
        (void)event.motion.x;
        (void)event.motion.y;
        break;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------

bool MOpenGLWindow::renderFrame()
{
    if (!m_sdlWindow || !m_glContext || !m_glInitialized) {
        return !m_pendingClose;
    }

    SDL_GL_MakeCurrent(m_sdlWindow, m_glContext);

    // Query the actual drawable size (may differ from window size on HiDPI).
    int fbWidth = 0, fbHeight = 0;
    SDL_GetWindowSizeInPixels(m_sdlWindow, &fbWidth, &fbHeight);

    gl::Viewport(0, 0, fbWidth, fbHeight);
    m_renderer->setProjection(static_cast<float>(fbWidth),
                              static_cast<float>(fbHeight));
    gl::ClearColor(0.125f, 0.125f, 0.137f, 1.0f); // Fluent Dark theme background
    gl::Clear(GL_COLOR_BUFFER_BIT);

    // Render the draw list.
    renderDrawList();

    SDL_GL_SwapWindow(m_sdlWindow);
    return !m_pendingClose;
}

void MOpenGLWindow::renderDrawList()
{
    if (!m_renderer || m_drawList.empty()) {
        return;
    }

    m_renderer->beginFrame();

    for (const DrawNode& node : m_drawList) {
        switch (node.type) {

        case DrawNode::Button: {
            // Fluent Design button: rounded rect with accent / reveal fill.
            constexpr float kButtonRadius = 4.0f;
            constexpr float kAccentR = 0.0f;
            constexpr float kAccentG = 0.376f;
            constexpr float kAccentB = 0.686f;
            constexpr float kAccentA = 1.0f;

            m_renderer->drawRoundedRect(
                static_cast<float>(node.rect.x()),
                static_cast<float>(node.rect.y()),
                static_cast<float>(node.rect.width()),
                static_cast<float>(node.rect.height()),
                kButtonRadius,
                kAccentR, kAccentG, kAccentB, kAccentA);

            // Draw button label text centered in the rect.
#ifndef MULU_NO_TEXT_RENDERER
            if (!node.text.isEmpty() && m_textRenderer) {
                const float textW = node.text.toStdString().size() * 10.0f; // rough estimate
                const float textH = 20.0f;
                const float tx = node.rect.x() + (node.rect.width()  - textW) * 0.5f;
                const float ty = node.rect.y() + (node.rect.height() - textH) * 0.5f;
                m_textRenderer->drawText(*m_renderer, node.text, tx, ty, 1.0f, 1.0f, 1.0f, 1.0f);
            }
#endif
            break;
        }

        case DrawNode::Label: {
            // Labels: just text (no background fill in default style).
#ifndef MULU_NO_TEXT_RENDERER
            if (!node.text.isEmpty() && m_textRenderer) {
                const float tx = static_cast<float>(node.rect.x());
                const float ty = static_cast<float>(node.rect.y());
                m_textRenderer->drawText(*m_renderer, node.text,
                                         tx, ty, 1.0f, 1.0f, 1.0f, 1.0f);
            }
#endif
            break;
        }

        case DrawNode::StackLayout: {
            // StackLayout itself is invisible; its children are drawn separately.
            break;
        }

        case DrawNode::None:
        default:
            break;
        }
    }

    m_renderer->endFrame();
}

} // namespace mulu
