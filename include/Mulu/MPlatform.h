#pragma once

#include "Mulu/MSize.h"
#include "Mulu/MString.h"

namespace mulu {

class MWindow;
class MWidget;

// ---------------------------------------------------------------------------
// MPlatformWindow
//
// A native window owned by a specific backend. The logical MWindow holds one
// of these; every public call on MWindow is forwarded here.
// ---------------------------------------------------------------------------
class MPlatformWindow {
public:
    virtual ~MPlatformWindow() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void close() = 0;

    virtual void setTitle(const MString& title) = 0;
    virtual MString title() const = 0;

    virtual void setSize(const MSize& size) = 0;
    virtual MSize size() const = 0;

    // Opaque handle to the underlying native window (HWND / GdkWindow / ...).
    virtual void* nativeHandle() = 0;

    // Rebuild the native widget tree to match the logical MWidget tree.
    virtual void syncWidgetTree(MWidget* root) = 0;
};

// ---------------------------------------------------------------------------
// MPlatformBackend
//
// Abstraction over a native GUI toolkit (WinUI3, GTK/Qt-style toolkits on
// Linux, SwiftUI on macOS, ...). Each supported platform implements this
// interface and is produced by the factory below.
// ---------------------------------------------------------------------------
class MPlatformBackend {
public:
    virtual ~MPlatformBackend() = default;

    // Initialize the underlying native toolkit. argc/argv mirror main().
    // Returns false if the toolkit could not be initialized.
    virtual bool initialize(int argc, char** argv) = 0;

    // Run the native event loop. Blocks until quit() is called.
    virtual int runEventLoop() = 0;

    // Ask the native event loop to terminate.
    virtual void quit() = 0;

    // Create/destroy a native window bound to the given logical MWindow.
    virtual MPlatformWindow* createWindow(MWindow* owner) = 0;
    virtual void destroyWindow(MPlatformWindow* window) = 0;

    // Application metadata exposed to the native toolkit.
    virtual void setApplicationName(const MString& name) = 0;
    virtual MString applicationName() const = 0;
};

// Backend factory. Resolves the backend for the current platform, or for an
// explicitly requested one (e.g. "windows", "linux", "macos"). Returns
// nullptr when no backend is available.
MPlatformBackend* createPlatformBackend(const MString& platformName = MString());

} // namespace mulu
