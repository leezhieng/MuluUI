#include "Mulu/MApplication.h"

#include "Mulu/MPlatform.h"

namespace mulu {

MApplication* MApplication::s_instance = nullptr;

MApplication::MApplication(int& argc, char** argv)
    : MObject(nullptr),
      m_argc(argc),
      m_argv(argv)
{
    s_instance = this;
    m_backend.reset(createPlatformBackend());
}

MApplication::~MApplication()
{
    // Destroy the backend before the singleton is cleared.
    m_backend.reset();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

MApplication* MApplication::instance()
{
    return s_instance;
}

int MApplication::exec()
{
    if (!m_backend) {
        return 1;
    }
    if (!m_backend->initialize(m_argc, m_argv)) {
        return 1;
    }
    m_running = true;
    m_exitCode = m_backend->runEventLoop();
    m_running = false;
    return m_exitCode;
}

void MApplication::quit()
{
    if (m_backend) {
        m_backend->quit();
    }
}

void MApplication::setApplicationName(const MString& name)
{
    if (m_backend) {
        m_backend->setApplicationName(name);
    }
}

MString MApplication::applicationName() const
{
    return m_backend ? m_backend->applicationName() : MString("MuluApplication");
}

void MApplication::setBackend(std::unique_ptr<MPlatformBackend> backend)
{
    m_backend = std::move(backend);
}

} // namespace mulu
