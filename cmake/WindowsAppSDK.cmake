# cmake/WindowsAppSDK.cmake
#
# Self-contained CMake integration for the Microsoft Windows App SDK (WinUI3).
#
# The Windows App SDK is distributed ONLY through NuGet: there is no vcpkg
# port and its NuGet packages ship no CMake config files (they are pure
# MSBuild props/targets glue). To use it from CMake this module:
#
#   1. downloads nuget.exe (once) and restores the Microsoft.WindowsAppSDK
#      NuGet package tree into ${MULU_WINDOWSAPPSDK_ROOT},
#   2. locates cppwinrt.exe from the installed Windows SDK and runs it against
#      the App SDK winmd files to generate the C++/WinRT projection headers
#      (winrt/Microsoft.UI.Xaml.h etc.) into ${CMAKE_BINARY_DIR}/generated/winrt,
#   3. wires the generated headers, the App SDK package include folders, the
#      Windows App Runtime bootstrap import library and the required compiler
#      definitions onto the target.
#
# Public entry point:
#   mulu_enable_windows_app_sdk(<target>)
#     Attaches the Windows App SDK to <target> (restore + generate run once).
#
# Cache variables:
#   MULU_WINDOWSAPPSDK_VERSION  Windows App SDK NuGet version (default 2.3.1)
#   MULU_WINDOWSAPPSDK_ROOT     Where the NuGet packages are restored

set(MULU_WINDOWSAPPSDK_VERSION "2.3.1" CACHE STRING
    "Microsoft Windows App SDK (WinUI3) NuGet version to restore")
set(MULU_WINDOWSAPPSDK_ROOT "${CMAKE_BINARY_DIR}/_deps/windowsappsdk" CACHE PATH
    "Directory where the Windows App SDK NuGet packages are restored")

# Set once the NuGet restore + cppwinrt generation has completed for this
# build directory, so subsequent calls only re-apply the target wiring.
set(_MULU_WASDK_READY FALSE)

# ---------------------------------------------------------------------------
# Locate cppwinrt.exe from the Windows SDK (prefers the newest installed kit).
# ---------------------------------------------------------------------------
function(_mulu_find_cppwinrt out_var)
    set(_kits_root "C:/Program Files (x86)/Windows Kits/10")
    if(DEFINED ENV{WindowsSdkDir} AND EXISTS "$ENV{WindowsSdkDir}")
        set(_kits_root "$ENV{WindowsSdkDir}")
    endif()

    # Collect installed SDK version folders (e.g. "10.0.26100.0"), skipping
    # the arch folders that also live under <kits>/bin.
    set(_versions "")
    file(GLOB _bin_dirs "${_kits_root}/bin/*")
    foreach(_d IN LISTS _bin_dirs)
        if(IS_DIRECTORY "${_d}")
            get_filename_component(_v "${_d}" NAME)
            if(_v MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
                list(APPEND _versions "${_v}")
            endif()
        endif()
    endforeach()

    # Prefer the version the toolchain is actually targeting, then newest.
    set(_ordered "")
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        list(APPEND _ordered "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    endif()
    list(APPEND _ordered ${_versions})
    list(REMOVE_DUPLICATES _ordered)

    foreach(_v IN LISTS _ordered)
        set(_cand "${_kits_root}/bin/${_v}/x64/cppwinrt.exe")
        if(EXISTS "${_cand}")
            set(${out_var} "${_cand}" PARENT_SCOPE)
            message(STATUS "MuluUI: using cppwinrt from Windows SDK ${_v}")
            return()
        endif()
    endforeach()

    message(FATAL_ERROR
        "MuluUI: cppwinrt.exe not found under ${_kits_root}/bin/<version>/x64. "
        "Install the Windows SDK (included with Visual Studio) to build the "
        "WinUI3 backend.")
endfunction()

# ---------------------------------------------------------------------------
# Restore the Windows App SDK NuGet package tree (idempotent via marker file).
# ---------------------------------------------------------------------------
function(_mulu_restore_windows_app_sdk)
    set(_root "${MULU_WINDOWSAPPSDK_ROOT}")
    set(_ver "${MULU_WINDOWSAPPSDK_VERSION}")
    set(_marker "${_root}/.mulu-wasdk-${_ver}")
    set(_nuget "${CMAKE_BINARY_DIR}/_tools/nuget.exe")

    if(EXISTS "${_marker}")
        return()
    endif()

    if(NOT EXISTS "${_nuget}")
        message(STATUS "MuluUI: downloading nuget.exe ...")
        file(DOWNLOAD
            "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe"
            "${_nuget}"
            STATUS _dl_status
            SHOW_PROGRESS)
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            message(FATAL_ERROR
                "MuluUI: failed to download nuget.exe (${_dl_status}). "
                "Install NuGet and re-run, or restore the Microsoft.WindowsAppSDK "
                "package tree into ${_root} manually.")
        endif()
    endif()

    message(STATUS
        "MuluUI: restoring Microsoft.WindowsAppSDK ${_ver} via NuGet into ${_root} ...")
    file(MAKE_DIRECTORY "${_root}")
    execute_process(
        COMMAND "${_nuget}" install Microsoft.WindowsAppSDK
            -Version "${_ver}"
            -OutputDirectory "${_root}"
            -ExcludeVersion
            -DependencyVersion Highest
            -NonInteractive
        RESULT_VARIABLE _nr)
    if(NOT _nr EQUAL 0)
        message(FATAL_ERROR
            "MuluUI: NuGet restore of Microsoft.WindowsAppSDK failed (${_nr}). "
            "Check the network connection and that ${_nuget} works.")
    endif()

    file(WRITE "${_marker}" "Microsoft.WindowsAppSDK ${_ver} restored")
endfunction()

# ---------------------------------------------------------------------------
# Generate the C++/WinRT projection headers from the App SDK winmd files.
# ---------------------------------------------------------------------------
function(_mulu_generate_winrt_projection cppwinrt)
    set(_root "${MULU_WINDOWSAPPSDK_ROOT}")
    set(_gen "${CMAKE_BINARY_DIR}/generated/winrt")
    set(_marker "${_gen}/winrt/Microsoft.UI.Xaml.h")

    if(EXISTS "${_marker}")
        return()
    endif()

    # winmd inputs: Foundation (MddBootstrap + Microsoft.Windows.*), the
    # InteractiveExperiences Microsoft.UI winmd (newest target version),
    # WinUI (Microsoft.UI.Xaml / Microsoft.UI.Text) and WebView2's projection.
    set(_inputs
        "${_root}/Microsoft.WindowsAppSDK.Foundation/metadata"
        "${_root}/Microsoft.WindowsAppSDK.InteractiveExperiences/metadata/10.0.18362.0"
        "${_root}/Microsoft.WindowsAppSDK.WinUI/metadata"
        "${_root}/Microsoft.Web.WebView2/lib/Microsoft.Web.WebView2.Core.winmd")
    foreach(_in IN LISTS _inputs)
        if(NOT EXISTS "${_in}")
            message(FATAL_ERROR
                "MuluUI: expected Windows App SDK input is missing: ${_in}. "
                "Delete ${_root} and reconfigure to re-run the NuGet restore.")
        endif()
    endforeach()

    # Keep the cppwinrt command line short via a response file.
    set(_rsp "${CMAKE_BINARY_DIR}/_tools/wasdk-cppwinrt.rsp")
    set(_rsp_content "")
    foreach(_in IN LISTS _inputs)
        string(APPEND _rsp_content "-input \"${_in}\"\n")
    endforeach()
    string(APPEND _rsp_content "-reference sdk\n")
    string(APPEND _rsp_content "-output \"${_gen}\"\n")
    file(WRITE "${_rsp}" "${_rsp_content}")

    message(STATUS
        "MuluUI: generating C++/WinRT projection headers into ${_gen} ...")
    execute_process(
        COMMAND "${cppwinrt}" "@${_rsp}"
        RESULT_VARIABLE _cr)
    if(NOT _cr EQUAL 0)
        message(FATAL_ERROR
            "MuluUI: cppwinrt projection generation failed (${_cr}). "
            "Response file: ${_rsp}")
    endif()
    if(NOT EXISTS "${_marker}")
        message(FATAL_ERROR
            "MuluUI: cppwinrt completed but ${_marker} was not produced.")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Attach includes / definitions / libraries to the target.
# ---------------------------------------------------------------------------
function(_mulu_wasdk_apply target)
    set(_root "${MULU_WINDOWSAPPSDK_ROOT}")
    set(_gen "${CMAKE_BINARY_DIR}/generated/winrt")

    if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
        set(_arch "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_arch "x64")
    else()
        set(_arch "x86")
    endif()

    # Generated winrt/*.h + the App SDK package include folders
    # (MddBootstrap.h, WindowsAppSDK-VersionInfo.h, prebuilt *Interop.h).
    target_include_directories("${target}" PRIVATE
        "${_gen}"
        "${_root}/Microsoft.WindowsAppSDK.InteractiveExperiences/include"
        "${_root}/Microsoft.WindowsAppSDK.Foundation/include"
        "${_root}/Microsoft.WindowsAppSDK.WinUI/include"
        "${_root}/Microsoft.WindowsAppSDK.Runtime/include")

    target_compile_definitions("${target}" PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        _WIN32_WINNT=0x0A00)

    # Force-include a preamble that includes <windows.h> first and undefines
    # the legacy GetCurrentTime() macro, which otherwise collides with the
    # generated C++/WinRT methods (e.g. IStoryboard::GetCurrentTime).
    if(MSVC)
        # Absolute path: /FI is resolved against the compiler's working dir.
        set(_winui_preamble "${CMAKE_SOURCE_DIR}/src/platform/windows/MuluWinUI.h")
        target_compile_options("${target}" PRIVATE "/FI${_winui_preamble}")
    endif()

    # Windows App Runtime bootstrap (MddBootstrapInitialize/Shutdown) and the
    # core runtime library (WINRT_IMPL_*, activation factory, etc.) are linked
    # PUBLIC because MuluUI is a static library and final executables must link
    # them too.
    set(_bootstrap_lib
        "${_root}/Microsoft.WindowsAppSDK.Foundation/lib/native/${_arch}/Microsoft.WindowsAppRuntime.Bootstrap.lib")
    set(_runtime_lib
        "${_root}/Microsoft.WindowsAppSDK.Foundation/lib/native/${_arch}/Microsoft.WindowsAppRuntime.lib")
    foreach(_lib IN ITEMS "${_bootstrap_lib}" "${_runtime_lib}")
        if(NOT EXISTS "${_lib}")
            message(FATAL_ERROR
                "MuluUI: required Windows App SDK library not found: ${_lib}")
        endif()
    endforeach()

    # onecoreuap.lib is the umbrella import library for the OneCore API
    # surface that Windows App SDK apps target.  It provides RoGetActivation-
    # Factory, RoOriginateLanguageException, and other WinRT base APIs that
    # are not in the default MSVC link set for classic desktop apps.
    target_link_libraries("${target}" PUBLIC
        "${_bootstrap_lib}" "${_runtime_lib}" onecoreuap.lib)
endfunction()

# ---------------------------------------------------------------------------
# Public entry point.
# ---------------------------------------------------------------------------
function(mulu_enable_windows_app_sdk target)
    if(_MULU_WASDK_READY)
        _mulu_wasdk_apply("${target}")
        return()
    endif()

    _mulu_restore_windows_app_sdk()
    _mulu_find_cppwinrt(_cppwinrt)
    _mulu_generate_winrt_projection("${_cppwinrt}")
    _mulu_wasdk_apply("${target}")

    set(_MULU_WASDK_READY TRUE PARENT_SCOPE)
endfunction()
