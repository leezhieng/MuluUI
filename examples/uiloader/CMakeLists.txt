add_executable(uiloader main.cpp)

target_link_libraries(uiloader PRIVATE MuluUI SDL3::SDL3)

# Bake the path to form.ui so the example always finds it regardless of CWD.
target_compile_definitions(uiloader PRIVATE
    MULU_FORM_UI="${CMAKE_CURRENT_SOURCE_DIR}/form.ui"
)

# Embed the Inter font.
include("${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake")
set(FONT_PATH "${CMAKE_SOURCE_DIR}/dependencies/font/Inter-Regular.ttf")
if(EXISTS "${FONT_PATH}")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    mulu_embed_binary(uiloader kFontData "${FONT_PATH}"
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(uiloader PRIVATE MULU_HAS_EMBEDDED_FONT=1)
endif()

if(WIN32)
    add_custom_command(TARGET uiloader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:uiloader>"
        COMMENT "Copying SDL3.dll to build directory"
    )
endif()

if(WIN32 AND MSVC)
    target_compile_options(uiloader PRIVATE /EHsc /Zc:preprocessor /permissive-)
endif()
