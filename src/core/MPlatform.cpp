#include "Mulu/MPlatform.h"

#include "MOpenGLPlatform.h"

namespace mulu {

MPlatformBackend* createPlatformBackend(const MString& platformName)
{
    // The SDL3 + OpenGL 3.3 backend is the only backend.
    // The platformName parameter is accepted for API compatibility but ignored.
    (void)platformName;
    return new MOpenGLPlatform();
}

} // namespace mulu
