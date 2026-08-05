cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Auto-detect the vcpkg CMake toolchain from the VCPKG_ROOT environment
# variable (or from common checkout locations).  This block must run before
# the first project() call so that the toolchain can set up the triplet,
# compiler paths, etc.
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # 1) VCPKG_ROOT environment variable
    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_candidate "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${_vcpkg_candidate}")
            set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_candidate}" CACHE FILEPATH
                "vcpkg CMake toolchain (auto-detected from VCPKG_ROOT)")
            message(STATUS "MuluUI: vcpkg toolchain detected via VCPKG_ROOT:")
            message(STATUS "         ${CMAKE_TOOLCHAIN_FILE}")
        endif()
    endif()

    # 2) Fall back to common checkout locations (sibling dir, in-project, ~/vcpkg)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        foreach(_dir IN ITEMS
            "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg"
            "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg"
            "$ENV{HOME}/vcpkg"
            "$ENV{USERPROFILE}/vcpkg"
        )
            set(_candidate "${_dir}/scripts/buildsystems/vcpkg.cmake")
            if(EXISTS "${_candidate}")
                set(CMAKE_TOOLCHAIN_FILE "${_candidate}" CACHE FILEPATH
                    "vcpkg CMake toolchain (auto-detected)")
                message(STATUS "MuluUI: vcpkg toolchain found at ${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "MuluUI: building with vcpkg dependencies")
    else()
        message(STATUS "MuluUI: vcpkg not found; core dependencies (e.g. tinyxml2)")
        message(STATUS "         will not be available unless provided externally.")
        message(STATUS "         Install vcpkg and set VCPKG_ROOT, or run")
        message(STATUS "         build_sdk.py --install-vcpkg to auto-install it.")
    endif()
endif()

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
# Core dependencies
# ---------------------------------------------------------------------------
# tinyxml2 powers the Qt Designer .ui form loader (MUiLoader). Linked PUBLIC
# because MuluUI is a static library and consumers need it too.
find_package(tinyxml2 CONFIG QUIET)

# ---------------------------------------------------------------------------
# MuluUI core library
# ---------------------------------------------------------------------------
set(MULU_CORE_SOURCES
    src/core/MObject.cpp
    src/core/MApplication.cpp
    src/core/MPlatform.cpp
    src/core/MWindow.cpp
    src/core/MWidget.cpp
    src/core/MLabel.cpp
    src/core/MButton.cpp
    src/core/MStackLayout.cpp
)

# MUiLoader requires tinyxml2; exclude it when tinyxml2 is unavailable.
if(tinyxml2_FOUND)
    list(APPEND MULU_CORE_SOURCES src/core/MUiLoader.cpp)
endif()

add_library(MuluUI ${MULU_CORE_SOURCES})

target_include_directories(MuluUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(MuluUI PUBLIC cxx_std_20)

if(tinyxml2_FOUND)
    message(STATUS "MuluUI: tinyxml2 found – MUiLoader (.ui form loader) enabled")
    target_link_libraries(MuluUI PUBLIC tinyxml2::tinyxml2)
    target_compile_definitions(MuluUI PUBLIC MULU_HAS_TINYXML2=1)
else()
    message(WARNING
        "MuluUI: tinyxml2 not found – MUiLoader (.ui form loader) will be "
        "unavailable. Install tinyxml2 via vcpkg (vcpkg install tinyxml2) or "
        "your system package manager."
    )
endif()

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

    # Windows App SDK (WinUI3). There is no vcpkg port and no CMake config in
    # the NuGet packages, so cmake/WindowsAppSDK.cmake restores the package
    # tree via NuGet, generates the C++/WinRT projection headers with cppwinrt
    # and wires up includes + the Windows App Runtime bootstrap library.
    include(cmake/WindowsAppSDK.cmake)
    mulu_enable_windows_app_sdk(MuluUI)

    target_compile_definitions(MuluUI PRIVATE MULU_PLATFORM_WINDOWS=1)

    if(MSVC)
        target_compile_options(MuluUI PRIVATE
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
