#pragma once

// ---------------------------------------------------------------------------
// GLFuncs – Minimal OpenGL 3.3 Core function loader for the SDL3 backend.
//
// SDL3 ships basic GL type definitions (GLuint, GLenum, GLAPIENTRY, …) in
// SDL_opengl.h, but its SDL_opengl_glext.h uses EXT‑style declarations
// (glGenTexturesEXT) rather than the standard PFNGL*PROC typedefs.  We
// therefore define every function‑pointer type we need right here.
//
// Call loadGLFunctions() once after an OpenGL 3.3 Core context is current.
// ---------------------------------------------------------------------------

#include <SDL3/SDL_opengl.h>   // GLuint, GLenum, GLsizei, GLfloat, GLint, …
                                // GLAPIENTRY, GL_FLOAT, GL_TRIANGLES, …

namespace mulu {
namespace gl {

// -----------------------------------------------------------------------
// Macro to declare one function pointer variable + its typedef in one line
// -----------------------------------------------------------------------
#define MULU_GL_FUNC(ret, name, params)            \
    using name##_t = ret (GLAPIENTRY *)params;      \
    extern name##_t name

// --- Shader compilation ----------------------------------------------------

MULU_GL_FUNC(GLuint, CreateShader,        (GLenum type));
MULU_GL_FUNC(void,   ShaderSource,        (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length));
MULU_GL_FUNC(void,   CompileShader,       (GLuint shader));
MULU_GL_FUNC(void,   GetShaderiv,         (GLuint shader, GLenum pname, GLint* params));
MULU_GL_FUNC(void,   GetShaderInfoLog,    (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
MULU_GL_FUNC(void,   DeleteShader,        (GLuint shader));

MULU_GL_FUNC(GLuint, CreateProgram,       (void));
MULU_GL_FUNC(void,   AttachShader,        (GLuint program, GLuint shader));
MULU_GL_FUNC(void,   LinkProgram,         (GLuint program));
MULU_GL_FUNC(void,   GetProgramiv,        (GLuint program, GLenum pname, GLint* params));
MULU_GL_FUNC(void,   GetProgramInfoLog,   (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
MULU_GL_FUNC(void,   UseProgram,          (GLuint program));
MULU_GL_FUNC(void,   DeleteProgram,       (GLuint program));

// --- Vertex arrays & buffers -----------------------------------------------

MULU_GL_FUNC(void,   GenVertexArrays,     (GLsizei n, GLuint* arrays));
MULU_GL_FUNC(void,   BindVertexArray,     (GLuint array));
MULU_GL_FUNC(void,   DeleteVertexArrays,  (GLsizei n, const GLuint* arrays));

MULU_GL_FUNC(void,   GenBuffers,          (GLsizei n, GLuint* buffers));
MULU_GL_FUNC(void,   BindBuffer,          (GLenum target, GLuint buffer));
MULU_GL_FUNC(void,   BufferData,          (GLenum target, GLsizeiptr size, const void* data, GLenum usage));
MULU_GL_FUNC(void,   BufferSubData,       (GLenum target, GLintptr offset, GLsizeiptr size, const void* data));
MULU_GL_FUNC(void,   DeleteBuffers,       (GLsizei n, const GLuint* buffers));

// --- Vertex attributes -----------------------------------------------------

MULU_GL_FUNC(void,   VertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer));
MULU_GL_FUNC(void,   EnableVertexAttribArray,  (GLuint index));
MULU_GL_FUNC(void,   DisableVertexAttribArray, (GLuint index));

// --- Uniforms --------------------------------------------------------------

MULU_GL_FUNC(GLint,   GetUniformLocation,  (GLuint program, const GLchar* name));
MULU_GL_FUNC(void,    Uniform1i,           (GLint location, GLint v0));
MULU_GL_FUNC(void,    Uniform1f,           (GLint location, GLfloat v0));
MULU_GL_FUNC(void,    Uniform2f,           (GLint location, GLfloat v0, GLfloat v1));
MULU_GL_FUNC(void,    Uniform3f,           (GLint location, GLfloat v0, GLfloat v1, GLfloat v2));
MULU_GL_FUNC(void,    Uniform4f,           (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
MULU_GL_FUNC(void,    UniformMatrix4fv,    (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value));

// --- Textures --------------------------------------------------------------

MULU_GL_FUNC(void,   GenTextures,         (GLsizei n, GLuint* textures));
MULU_GL_FUNC(void,   BindTexture,         (GLenum target, GLuint texture));
MULU_GL_FUNC(void,   TexImage2D,          (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels));
MULU_GL_FUNC(void,   TexParameteri,       (GLenum target, GLenum pname, GLint param));
MULU_GL_FUNC(void,   TexSubImage2D,       (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels));
MULU_GL_FUNC(void,   DeleteTextures,      (GLsizei n, const GLuint* textures));
MULU_GL_FUNC(void,   ActiveTexture,       (GLenum texture));

// --- Render state ----------------------------------------------------------

MULU_GL_FUNC(void,   Enable,              (GLenum cap));
MULU_GL_FUNC(void,   Disable,             (GLenum cap));
MULU_GL_FUNC(void,   BlendFunc,           (GLenum sfactor, GLenum dfactor));
MULU_GL_FUNC(void,   BlendFuncSeparate,   (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha));
MULU_GL_FUNC(void,   Viewport,            (GLint x, GLint y, GLsizei width, GLsizei height));
MULU_GL_FUNC(void,   Clear,               (GLbitfield mask));
MULU_GL_FUNC(void,   ClearColor,          (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
MULU_GL_FUNC(void,   Scissor,             (GLint x, GLint y, GLsizei width, GLsizei height));

// --- Drawing ---------------------------------------------------------------

MULU_GL_FUNC(void,   DrawElements,        (GLenum mode, GLsizei count, GLenum type, const void* indices));
MULU_GL_FUNC(void,   DrawArrays,          (GLenum mode, GLint first, GLsizei count));

// --- Misc ------------------------------------------------------------------

MULU_GL_FUNC(void,   PixelStorei,         (GLenum pname, GLint param));
MULU_GL_FUNC(GLenum, GetError,            (void));
MULU_GL_FUNC(void,   ReadPixels,          (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels));

#undef MULU_GL_FUNC

// Load all function pointers declared above via SDL_GL_GetProcAddress.
// Must be called with a current OpenGL 3.3 Core context.
// Returns true on success, false if any required function is unavailable.
bool loadGLFunctions();

} // namespace gl
} // namespace mulu
