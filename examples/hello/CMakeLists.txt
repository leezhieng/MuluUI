add_executable(hello main.cpp)

target_link_libraries(hello PRIVATE MuluUI)

# The Windows App SDK (WinUI3) + Windows App Runtime bootstrap library come
# from MuluUI (linked PUBLIC there because MuluUI is a static library), so
# this example needs no extra find_package calls.

if(WIN32 AND MSVC)
    target_compile_options(hello PRIVATE /EHsc /Zc:preprocessor /permissive- /bigobj)
endif()
