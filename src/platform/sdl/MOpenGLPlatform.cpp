#include "MOpenGLPlatform.h"

#include <algorithm>

#include <SDL3/SDL.h>

#include "MOpenGLWindow.h"
#include "Mulu/MWindow.h"

namespace mulu {

MOpenGLPlatform* MOpenGLPlatform::s_instance = nullptr;

MOpenGLPlatform::MOpenGLPlatform()
{
    s_instance = this;
}

MOpenGLPlatform::~MOpenGLPlatform()
{
    // Destroy remaining windows before shutting down SDL.
    while (!m_windows.empty()) {
        // destroyWindow removes from the vector; operate on the first element.
        MPlatformWindow* w = m_windows.front();
        destroyWindow(w);
    }

    if (s_instance == this) {
        s_instance = nullptr;
    }
    SDL_Quit();
}

MOpenGLPlatform* MOpenGLPlatform::instance()
{
    return s_instance;
}

// ---------------------------------------------------------------------------
// Backend lifecycle
// ---------------------------------------------------------------------------

bool MOpenGLPlatform::initialize(int /*argc*/, char** /*argv*/)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("MOpenGLPlatform: SDL_Init(SDL_INIT_VIDEO) failed: %s",
                SDL_GetError());
        return false;
    }

    SDL_Log("MOpenGLPlatform: SDL3 video subsystem initialized");
    return true;
}

int MOpenGLPlatform::runEventLoop()
{
    m_running = true;
    m_exitCode = 0;

    SDL_Event event;
    while (m_running) {
        // Process all pending events.
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;

            default: {
                // Route the event to its owning window by event type.
                uint32_t wid = 0;
                if (event.type >= SDL_EVENT_WINDOW_FIRST &&
                    event.type <= SDL_EVENT_WINDOW_LAST) {
                    wid = event.window.windowID;
                } else if (event.type >= SDL_EVENT_MOUSE_MOTION &&
                           event.type <= SDL_EVENT_MOUSE_REMOVED) {
                    wid = event.motion.windowID;
                } else if (event.type >= SDL_EVENT_KEY_DOWN &&
                           event.type <= SDL_EVENT_KEYMAP_CHANGED) {
                    wid = event.key.windowID;
                } else if (event.type == SDL_EVENT_TEXT_INPUT) {
                    wid = event.text.windowID;
                }

                if (MOpenGLWindow* win = findWindowBySDLID(wid)) {
                    win->handleSDLEvent(event);
                    m_needsRender = true;
                }
                break;
            }
            }
        }

        // Poll window close requests.
        for (auto it = m_windows.begin(); it != m_windows.end(); ) {
            MOpenGLWindow* win = *it;
            if (win && win->isCloseRequested()) {
                // Notify the logical window, which will trigger destroyWindow.
                MWindow* owner = win->owner();
                if (owner) {
                    owner->handleClose();
                }
                // handleClose() may have already destroyed the window;
                // re-validate the iterator.
                if (std::find(m_windows.begin(), m_windows.end(), win) ==
                    m_windows.end()) {
                    it = m_windows.begin(); // restart scan
                    continue;
                }
            }
            ++it;
        }

        // Render all visible windows.
        if (m_needsRender) {
            renderAllWindows();
            m_needsRender = false;
        }

        // Small sleep to avoid busy-waiting at 100% CPU when idle.
        SDL_Delay(1);
    }

    return m_exitCode;
}

void MOpenGLPlatform::quit()
{
    m_running = false;
    m_exitCode = 0;
}

// ---------------------------------------------------------------------------
// Window management
// ---------------------------------------------------------------------------

MPlatformWindow* MOpenGLPlatform::createWindow(MWindow* owner)
{
    auto* win = new MOpenGLWindow(owner);
    m_windows.push_back(win);
    SDL_Log("MOpenGLPlatform: created window (total=%zu)", m_windows.size());
    return win;
}

void MOpenGLPlatform::destroyWindow(MPlatformWindow* window)
{
    auto* sdlWin = dynamic_cast<MOpenGLWindow*>(window);
    if (!sdlWin) {
        return;
    }

    m_windows.erase(
        std::remove(m_windows.begin(), m_windows.end(), sdlWin),
        m_windows.end());

    delete sdlWin;
    m_needsRender = true;
    SDL_Log("MOpenGLPlatform: destroyed window (total=%zu)", m_windows.size());

    // If no windows remain, exit the event loop.
    if (m_windows.empty()) {
        m_running = false;
    }
}

// ---------------------------------------------------------------------------
// Window registry helpers
// ---------------------------------------------------------------------------

void MOpenGLPlatform::registerWindow(MOpenGLWindow* /*window*/)
{
    // Windows register themselves during construction via createWindow().
}

void MOpenGLPlatform::unregisterWindow(MOpenGLWindow* /*window*/)
{
    // Windows unregister during destruction via destroyWindow().
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void MOpenGLPlatform::renderAllWindows()
{
    for (auto it = m_windows.begin(); it != m_windows.end(); ) {
        MOpenGLWindow* win = *it;
        if (!win || !win->isVisible()) {
            ++it;
            continue;
        }

        if (!win->renderFrame()) {
            // Window asked to close.
            MWindow* owner = win->owner();
            if (owner) {
                owner->handleClose();
            }
            if (std::find(m_windows.begin(), m_windows.end(), win) ==
                m_windows.end()) {
                it = m_windows.begin();
                continue;
            }
        }
        ++it;
    }
}

MOpenGLWindow* MOpenGLPlatform::findWindowBySDLID(uint32_t sdlWindowID) const
{
    for (auto* win : m_windows) {
        if (win && win->sdlWindowID() == sdlWindowID) {
            return win;
        }
    }
    return nullptr;
}

} // namespace mulu
