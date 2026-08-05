#pragma once

#include <vector>

#include "Mulu/MPlatform.h"
#include "Mulu/MString.h"

namespace mulu {

class MOpenGLWindow;

// ---------------------------------------------------------------------------
// MOpenGLPlatform – SDL3 + OpenGL 3.3 backend for MuluUI.
//
// Owns the SDL3 video subsystem, drives the event loop, and manages a list
// of MOpenGLWindow instances.  Each frame polls SDL events, dispatches them
// to the target window, and re-renders all visible windows.
// ---------------------------------------------------------------------------
class MOpenGLPlatform : public MPlatformBackend {
public:
    MOpenGLPlatform();
    ~MOpenGLPlatform() override;

    // --- MPlatformBackend interface -----------------------------------------

    bool initialize(int argc, char** argv) override;
    int  runEventLoop() override;
    void quit() override;

    MPlatformWindow* createWindow(MWindow* owner) override;
    void destroyWindow(MPlatformWindow* window) override;

    void    setApplicationName(const MString& name) override { m_appName = name; }
    MString applicationName() const override { return m_appName; }

    // --- Window registry (used by MOpenGLWindow) ----------------------------

    void registerWindow(MOpenGLWindow* window);
    void unregisterWindow(MOpenGLWindow* window);

    // Flag a re-render for the next frame.
    void requestRender() { m_needsRender = true; }

    static MOpenGLPlatform* instance();

private:
    void renderAllWindows();
    MOpenGLWindow* findWindowBySDLID(uint32_t sdlWindowID) const;

    MString m_appName = "MuluApplication";
    bool    m_running = false;
    bool    m_needsRender = true;
    int     m_exitCode = 0;
    std::vector<MOpenGLWindow*> m_windows;

    static MOpenGLPlatform* s_instance;
};

} // namespace mulu
