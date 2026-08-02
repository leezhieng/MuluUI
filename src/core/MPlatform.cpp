#include "Mulu/MPlatform.h"

#if defined(MULU_PLATFORM_WINDOWS)
#include "MWindowsPlatform.h"
#endif

namespace mulu {

MPlatformBackend* createPlatformBackend(const MString& platformName)
{
#if defined(MULU_PLATFORM_WINDOWS)
    if (platformName.isEmpty() || platformName == "windows" ||
        platformName == "winui") {
        return new MWindowsPlatform();
    }
#else
    (void)platformName;
#endif
    // No backend for this platform yet (Linux/macOS backends are on the
    // roadmap). MApplication::exec() will return 1 in that case.
    return nullptr;
}

} // namespace mulu
