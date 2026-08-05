# cmake/EmbedBinary.cmake
#
# Provides `mulu_embed_binary` — a function that converts an arbitrary binary
# file into a C++ source file at build time and compiles it into a target.
#
# Usage:
#   mulu_embed_binary(<target> <variable_name> <binary_path>
#                     [OUTPUT_DIR <dir>])
#
# Example:
#   mulu_embed_binary(hello kFontData "${CMAKE_SOURCE_DIR}/resources/font.ttf"
#                     OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated")
#
# After calling this, <variable_name> and <variable_name>Size are available as:
#   extern const unsigned char <variable_name>[];
#   extern const unsigned int  <variable_name>Size;

function(mulu_embed_binary target var_name binary_path)
    cmake_parse_arguments(ARG "" "OUTPUT_DIR" "" ${ARGN})

    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    endif()

    get_filename_component(_base "${binary_path}" NAME_WE)
    set(_output_cpp "${ARG_OUTPUT_DIR}/${_base}_embedded.cpp")

    add_custom_command(
        OUTPUT "${_output_cpp}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/embed_binary.py"
                "${binary_path}"
                "${_output_cpp}"
                "${var_name}"
        DEPENDS "${binary_path}"
                "${CMAKE_SOURCE_DIR}/tools/embed_binary.py"
        COMMENT "Embedding ${binary_path} → ${_output_cpp}"
    )

    target_sources("${target}" PRIVATE "${_output_cpp}")
    target_include_directories("${target}" PRIVATE "${ARG_OUTPUT_DIR}")

    # Declare the extern variables so callers can use them after including
    # the generated header (or by declaring extern themselves).
    message(STATUS "  Embedded '${var_name}' (${binary_path})")
endfunction()
