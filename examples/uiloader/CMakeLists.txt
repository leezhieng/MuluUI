add_executable(uiloader main.cpp)

target_link_libraries(uiloader PRIVATE MuluUI)

# Bake the path to form.ui so the example always finds it regardless of CWD.
target_compile_definitions(uiloader PRIVATE
    MULU_FORM_UI="${CMAKE_CURRENT_SOURCE_DIR}/form.ui"
)

# The Windows App SDK (WinUI3) + Windows App Runtime bootstrap library come
# from MuluUI (linked PUBLIC there because MuluUI is a static library), so
# this example needs no extra find_package calls.

if(WIN32 AND MSVC)
    target_compile_options(uiloader PRIVATE /EHsc /Zc:preprocessor /permissive- /bigobj)
endif()
