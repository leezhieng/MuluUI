add_executable(MuluSDLHello main.cpp)

target_link_libraries(MuluSDLHello PRIVATE MuluUI SDL3::SDL3)

# Embed the Inter font.
include("${CMAKE_SOURCE_DIR}/cmake/EmbedBinary.cmake")
set(FONT_PATH "${CMAKE_SOURCE_DIR}/dependencies/font/Inter-Regular.ttf")
if(EXISTS "${FONT_PATH}")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    mulu_embed_binary(MuluSDLHello kFontData "${FONT_PATH}"
        OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(MuluSDLHello PRIVATE MULU_HAS_EMBEDDED_FONT=1)
endif()

if(WIN32)
    add_custom_command(TARGET MuluSDLHello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:MuluSDLHello>"
        COMMENT "Copying SDL3.dll to build directory"
    )
endif()
