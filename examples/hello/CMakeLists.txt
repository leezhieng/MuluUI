add_executable(hello main.cpp)

target_link_libraries(hello PRIVATE MuluUI)

if(WIN32)
    # The final executable must also carry the Windows App SDK + bootstrap
    # dependencies (MuluUI is a static library, so its private deps do not
    # propagate).
    find_package(Microsoft.WindowsAppSDK CONFIG REQUIRED)
    target_link_libraries(hello PRIVATE Microsoft.WindowsAppSDK)

    find_package(Microsoft.WindowsAppRuntime.Bootstrap CONFIG QUIET)
    if(Microsoft.WindowsAppRuntime.Bootstrap_FOUND)
        target_link_libraries(hello PRIVATE Microsoft.WindowsAppRuntime.Bootstrap)
    endif()
endif()

if(WIN32 AND MSVC)
    target_compile_options(hello PRIVATE /await /EHsc /Zc:preprocessor /permissive- /bigobj)
endif()
