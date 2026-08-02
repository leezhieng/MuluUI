add_executable(uiloader main.cpp)

target_link_libraries(uiloader PRIVATE MuluUI)

# Bake the path to form.ui so the example always finds it regardless of CWD.
target_compile_definitions(uiloader PRIVATE
    MULU_FORM_UI="${CMAKE_CURRENT_SOURCE_DIR}/form.ui"
)

if(WIN32)
    # The final executable must also carry the Windows App SDK + bootstrap
    # dependencies (MuluUI is a static library, so its private deps do not
    # propagate).
    find_package(Microsoft.WindowsAppSDK CONFIG REQUIRED)
    target_link_libraries(uiloader PRIVATE Microsoft.WindowsAppSDK)

    find_package(Microsoft.WindowsAppRuntime.Bootstrap CONFIG QUIET)
    if(Microsoft.WindowsAppRuntime.Bootstrap_FOUND)
        target_link_libraries(uiloader PRIVATE Microsoft.WindowsAppRuntime.Bootstrap)
    endif()
endif()

if(WIN32 AND MSVC)
    target_compile_options(uiloader PRIVATE /await /EHsc /Zc:preprocessor /permissive- /bigobj)
endif()
