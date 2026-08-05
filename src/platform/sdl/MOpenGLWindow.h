#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Mulu/MPlatform.h"
#include "Mulu/MRect.h"
#include "Mulu/MSize.h"
#include "Mulu/MString.h"

// Forward-declare SDL3 types to avoid leaking SDL headers into the public API.
// SDL_Event needs its full definition (it's a union typedef), so we include
// the SDL header here — this is the platform layer, not the public API.
#include <SDL3/SDL.h>

struct SDL_Window;
struct SDL_GLContextState; // SDL_GLContext is a typedef for this opaque pointer

namespace mulu {

class MWindow;
class MWidget;
class MButton;
class MLabel;
class MStackLayout;
class GLRenderer;
#ifndef MULU_NO_TEXT_RENDERER
class TextRenderer;
#endif

// ---------------------------------------------------------------------------
// MOpenGLWindow – SDL3 + OpenGL 3.3 native window bound to a logical MWindow.
//
// Responsibilities:
//   • Own an SDL_Window with an OpenGL 3.3 Core context
//   • Convert the logical MWidget tree into a flat DrawNode list (syncWidgetTree)
//   • Render the draw list each frame via GLRenderer + TextRenderer
//   • Dispatch SDL input events → hit-test → MButton::click()
// ---------------------------------------------------------------------------
class MOpenGLWindow : public MPlatformWindow {
public:
    explicit MOpenGLWindow(MWindow* owner);
    ~MOpenGLWindow() override;

    // --- MPlatformWindow interface ------------------------------------------

    void show() override;
    void hide() override;
    void close() override;

    void    setTitle(const MString& title) override;
    MString title() const override { return m_title; }

    void  setSize(const MSize& size) override;
    MSize size() const override { return m_size; }

    void* nativeHandle() override;

    void syncWidgetTree(MWidget* root) override;

    // --- Embedded font ------------------------------------------------------
    // Set font data from an embedded resource (binary → C array via
    // cmake/EmbedBinary.cmake).  Must be called before show().
    void setFontData(const unsigned char* data, unsigned int size) override;

    // --- Frame rendering (called by MOpenGLPlatform) ------------------------
    // Returns false if the window should be closed.
    bool renderFrame();

    // --- SDL event dispatch (called by MOpenGLPlatform) ---------------------

    uint32_t sdlWindowID() const { return m_sdlWindowID; }
    void handleSDLEvent(const SDL_Event& event);

    // --- Queries ------------------------------------------------------------

    MWindow* owner() const          { return m_owner; }
    bool     isVisible() const      { return m_visible; }
    bool     isCloseRequested() const { return m_pendingClose; }

private:
    // --- Initialization -----------------------------------------------------

    bool createSDLWindow();

    // --- Draw-list construction ---------------------------------------------

    struct DrawNode {
        enum Type : uint8_t { None, Button, Label, StackLayout };
        Type    type = None;
        MRect   rect;           // in window-local pixel coordinates
        MString text;           // for Label / Button
        MButton* button = nullptr; // for Button: non-null to dispatch clicks
        int     parentIndex = -1;  // index of parent StackLayout, -1 for root
    };

    void buildDrawListRecursive(MWidget* widget, int parentIndex);

    // Recursively call layout() on every widget in the tree that has
    // children, so that child geometries are computed before the draw
    // list is built.
    static void layoutRecursive(MWidget* widget);

    // --- Hit-testing --------------------------------------------------------

    // Returns the index of the deepest DrawNode whose rect contains (x, y).
    // Returns -1 if no node matches.
    int hitTest(int x, int y) const;

    // --- Rendering a single frame -------------------------------------------

    void renderDrawList();

    // --- Data members -------------------------------------------------------

    MWindow* m_owner = nullptr;
    MString  m_title;
    MSize    m_size{800, 600};
    bool     m_visible    = false;
    bool     m_pendingClose = false;

    // SDL3 resources
    SDL_Window*         m_sdlWindow   = nullptr;
    SDL_GLContextState* m_glContext   = nullptr;
    uint32_t            m_sdlWindowID = 0;
    bool                m_glInitialized = false;

    // Renderer subsystems (owning pointers)
    std::unique_ptr<GLRenderer>   m_renderer;
#ifndef MULU_NO_TEXT_RENDERER
    std::unique_ptr<TextRenderer> m_textRenderer;
    const unsigned char* m_fontData = nullptr;
    unsigned int         m_fontDataSize = 0;
#endif

    // Retained draw list – rebuilt by syncWidgetTree(), rendered each frame.
    std::vector<DrawNode> m_drawList;

    // Guard against re-entrant syncWidgetTree calls when setGeometry()
    // triggers requestWidgetSync during the layout pass.
    bool m_syncInProgress = false;
};

} // namespace mulu
