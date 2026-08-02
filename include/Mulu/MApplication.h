#pragma once

#include <functional>
#include <memory>

#include "Mulu/MObject.h"
#include "Mulu/MString.h"
#include "Mulu/MSize.h"
#include "Mulu/MPoint.h"

namespace mulu {

class MPlatformBackend;

// Owns the platform backend and drives the native event loop.
// Mirrors QApplication: create exactly one instance at the start of main().
class MApplication : public MObject {
public:
    explicit MApplication(int& argc, char** argv);
    ~MApplication() override;

    static MApplication* instance();

    // Runs the native event loop until quit() is called. Returns the exit code.
    int exec();
    void quit();

    void setApplicationName(const MString& name);
    MString applicationName() const;

    // Inject a custom backend (used by tests or to force a specific toolkit).
    void setBackend(std::unique_ptr<MPlatformBackend> backend);

    MPlatformBackend* backend() const { return m_backend.get(); }

private:
    int m_argc = 0;
    char** m_argv = nullptr;
    std::unique_ptr<MPlatformBackend> m_backend;
    int m_exitCode = 0;
    bool m_running = false;

    static MApplication* s_instance;
};

} // namespace mulu
