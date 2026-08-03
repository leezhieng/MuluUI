#pragma once
// Forced-include preamble for translation units that use the WinUI3 /
// Windows App SDK C++/WinRT projection headers (wired in via /FI in
// cmake/WindowsAppSDK.cmake).
//
// The generated C++/WinRT headers (e.g. Microsoft.UI.Xaml.Media.Animation)
// declare methods named GetCurrentTime(), which collides with the legacy
// Win32 function-like macro `#define GetCurrentTime() GetTickCount()` from
// WinBase.h. Include <windows.h> first and undefine the macro so the
// projection headers compile cleanly regardless of include order.
#include <windows.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
