cmake_minimum_required(VERSION 3.22)

project(MuluUI
    VERSION 0.1.0
    DESCRIPTION "Cross-platform C++ GUI SDK wrapping native toolkits"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# MuluUI core library
# ---------------------------------------------------------------------------
add_library(MuluUI
    src/core/MObject.cpp
    src/core/MApplication.cpp
    src/core/MPlatform.cpp
    src/core/MWindow.cpp
    src/core/MWidget.cpp
    src/core/MLabel.cpp
    src/core/MButton.cpp
    src/core/MStackLayout.cpp
    src/core/MUiLoader.cpp
)

target_include_directories(MuluUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(MuluUI PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# Core dependencies
# ---------------------------------------------------------------------------
# tinyxml2 powers the Qt Designer .ui form loader (MUiLoader). Linked PUBLIC
# because MuluUI is a static library and consumers need it too.
find_package(tinyxml2 CONFIG REQUIRED)
target_link_libraries(MuluUI PUBLIC tinyxml2::tinyxml2)

# ---------------------------------------------------------------------------
# Platform backends
#
# Windows-first: the WinUI3 backend is built whenever we are on Windows.
# Additional backends (Linux/GTK, macOS/SwiftUI) plug in here later.
# ---------------------------------------------------------------------------
if(WIN32)
    message(STATUS "MuluUI: enabling Windows (WinUI3) backend")

    target_sources(MuluUI PRIVATE
        src/platform/windows/MWindowsPlatform.cpp
        src/platform/windows/MWindowsWindow.cpp
        src/platform/windows/MWinUIApp.cpp
    )
    target_include_directories(MuluUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows
    )

    # Windows App SDK (WinUI3) provisioned via vcpkg (microsoft-windowsappsdk)
    # or the Microsoft.WindowsAppSDK NuGet package.
    find_package(Microsoft.WindowsAppSDK CONFIG REQUIRED)
    target_link_libraries(MuluUI PRIVATE Microsoft.WindowsAppSDK)

    # Bootstrap for unpackaged (self-contained) deployment.
    find_package(Microsoft.WindowsAppRuntime.Bootstrap CONFIG QUIET)
    if(Microsoft.WindowsAppRuntime.Bootstrap_FOUND)
        target_link_libraries(MuluUI PRIVATE Microsoft.WindowsAppRuntime.Bootstrap)
    endif()

    target_compile_definitions(MuluUI PRIVATE MULU_PLATFORM_WINDOWS=1)

    if(MSVC)
        target_compile_options(MuluUI PRIVATE
            /await
            /EHsc
            /Zc:preprocessor
            /permissive-
            /bigobj
        )
    endif()
else()
    message(WARNING
        "MuluUI: no backend for the current platform yet "
        "(Windows/WinUI3 is the first target; Linux/macOS backends are planned)"
    )
    target_compile_definitions(MuluUI PRIVATE MULU_PLATFORM_UNSUPPORTED=1)
endif()

# ---------------------------------------------------------------------------
# Install / export
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS MuluUI
    EXPORT MuluUITargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/Mulu DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT MuluUITargets
    FILE MuluUITargets.cmake
    NAMESPACE Mulu::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MuluUI
)

# ---------------------------------------------------------------------------
# Examples (only when MuluUI is the top-level project)
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(examples/hello)
endif()
