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
    DESCRIPTION "Cross-platform C++ GUI SDK with SDL3 + OpenGL 3.3 GPU renderer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: treat all source files as UTF-8 (suppresses C4819 warnings on systems
# where the locale codepage is not UTF-8, e.g. codepage 936 / GBK).
if(MSVC)
    add_compile_options(/utf-8)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Core dependencies
# ---------------------------------------------------------------------------
# CMAKE_PREFIX_PATH can be set by build_sdk.py to point at the locally-built
# dependencies in dependencies/install/ (produced by download_dep.py).
if(DEFINED CMAKE_PREFIX_PATH)
    message(STATUS "MuluUI: using dependency prefix: ${CMAKE_PREFIX_PATH}")
endif()

# tinyxml2 powers the Qt Designer .ui form loader (MUiLoader).
find_package(tinyxml2 CONFIG QUIET)

# SDL3 + OpenGL are the sole rendering backend.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
    message(FATAL_ERROR
        "MuluUI: SDL3 not found.\n"
        "Run download_dep.py to build it locally:\n"
        "    python download_dep.py\n"
        "Or install via vcpkg:\n"
        "    vcpkg install sdl3\n"
        "Or set SDL3_DIR to a pre-built SDL3 install prefix."
    )
endif()

find_package(OpenGL REQUIRED)

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
# SDL3 + OpenGL 3.3 backend
# ---------------------------------------------------------------------------
message(STATUS "MuluUI: enabling SDL3 + OpenGL 3.3 backend")

set(MULU_SDL_SOURCES
    src/platform/sdl/MOpenGLPlatform.cpp
    src/platform/sdl/MOpenGLWindow.cpp
    src/platform/sdl/GLRenderer.cpp
    src/platform/sdl/GLFuncs.cpp
)

# TextRenderer requires stb_truetype.h (single-header, public domain).
# download_dep.py places it in dependencies/install/include/.
set(_stb_path "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/install/include/stb_truetype.h")
if(EXISTS "${_stb_path}")
    message(STATUS "MuluUI: stb_truetype.h found — TextRenderer enabled")
    list(APPEND MULU_SDL_SOURCES src/platform/sdl/TextRenderer.cpp)
else()
    message(WARNING
        "MuluUI: stb_truetype.h not found at ${_stb_path} — "
        "Text rendering will be disabled.\n"
        "  Run download_dep.py to fetch it:\n"
        "    python download_dep.py"
    )
    target_compile_definitions(MuluUI PRIVATE MULU_NO_TEXT_RENDERER=1)
endif()

target_sources(MuluUI PRIVATE ${MULU_SDL_SOURCES})
target_include_directories(MuluUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/sdl
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/install/include
)

target_link_libraries(MuluUI PUBLIC
    SDL3::SDL3
    OpenGL::GL
)

target_compile_definitions(MuluUI PRIVATE MULU_PLATFORM_SDL_OPENGL=1)

if(MSVC)
    target_compile_options(MuluUI PRIVATE
        /EHsc
        /Zc:preprocessor
        /permissive-
        /utf-8
    )
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
    add_subdirectory(examples/sdlhello)
    if(tinyxml2_FOUND)
        add_subdirectory(examples/uiloader)
    endif()
endif()
