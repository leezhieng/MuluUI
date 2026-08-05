add_executable(hello main.cpp)

target_link_libraries(hello PRIVATE MuluUI SDL3::SDL3)

# Embed the Inter font (downloaded by download_dep.py) into the executable.
include("${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake")
set(FONT_PATH "${CMAKE_SOURCE_DIR}/dependencies/font/Inter-Regular.ttf")
if(EXISTS "${FONT_PATH}")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    mulu_embed_binary(hello kFontData "${FONT_PATH}"
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(hello PRIVATE MULU_HAS_EMBEDDED_FONT=1)
else()
    message(WARNING "Font not found at ${FONT_PATH} — run download_dep.py first")
endif()

# Copy the SDL3 DLL next to the executable on Windows.
if(WIN32)
    add_custom_command(TARGET hello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:hello>"
        COMMENT "Copying SDL3.dll to build directory"
    )
endif()

if(WIN32 AND MSVC)
    target_compile_options(hello PRIVATE /EHsc /Zc:preprocessor /permissive-)
endif()
