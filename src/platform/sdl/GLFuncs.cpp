#include "GLFuncs.h"

#include <SDL3/SDL.h>

namespace mulu {
namespace gl {

// -----------------------------------------------------------------------
// Define every function pointer (initialised to nullptr)
// -----------------------------------------------------------------------
#define MULU_GL_DEF(ret, name, params) name##_t name = nullptr

MULU_GL_DEF(GLuint, CreateShader,        (GLenum));
MULU_GL_DEF(void,   ShaderSource,        (GLuint, GLsizei, const GLchar* const*, const GLint*));
MULU_GL_DEF(void,   CompileShader,       (GLuint));
MULU_GL_DEF(void,   GetShaderiv,         (GLuint, GLenum, GLint*));
MULU_GL_DEF(void,   GetShaderInfoLog,    (GLuint, GLsizei, GLsizei*, GLchar*));
MULU_GL_DEF(void,   DeleteShader,        (GLuint));

MULU_GL_DEF(GLuint, CreateProgram,       (void));
MULU_GL_DEF(void,   AttachShader,        (GLuint, GLuint));
MULU_GL_DEF(void,   LinkProgram,         (GLuint));
MULU_GL_DEF(void,   GetProgramiv,        (GLuint, GLenum, GLint*));
MULU_GL_DEF(void,   GetProgramInfoLog,   (GLuint, GLsizei, GLsizei*, GLchar*));
MULU_GL_DEF(void,   UseProgram,          (GLuint));
MULU_GL_DEF(void,   DeleteProgram,       (GLuint));

MULU_GL_DEF(void,   GenVertexArrays,     (GLsizei, GLuint*));
MULU_GL_DEF(void,   BindVertexArray,     (GLuint));
MULU_GL_DEF(void,   DeleteVertexArrays,  (GLsizei, const GLuint*));

MULU_GL_DEF(void,   GenBuffers,          (GLsizei, GLuint*));
MULU_GL_DEF(void,   BindBuffer,          (GLenum, GLuint));
MULU_GL_DEF(void,   BufferData,          (GLenum, GLsizeiptr, const void*, GLenum));
MULU_GL_DEF(void,   BufferSubData,       (GLenum, GLintptr, GLsizeiptr, const void*));
MULU_GL_DEF(void,   DeleteBuffers,       (GLsizei, const GLuint*));

MULU_GL_DEF(void,   VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*));
MULU_GL_DEF(void,   EnableVertexAttribArray,  (GLuint));
MULU_GL_DEF(void,   DisableVertexAttribArray, (GLuint));

MULU_GL_DEF(GLint,   GetUniformLocation,  (GLuint, const GLchar*));
MULU_GL_DEF(void,    Uniform1i,           (GLint, GLint));
MULU_GL_DEF(void,    Uniform1f,           (GLint, GLfloat));
MULU_GL_DEF(void,    Uniform2f,           (GLint, GLfloat, GLfloat));
MULU_GL_DEF(void,    Uniform3f,           (GLint, GLfloat, GLfloat, GLfloat));
MULU_GL_DEF(void,    Uniform4f,           (GLint, GLfloat, GLfloat, GLfloat, GLfloat));
MULU_GL_DEF(void,    UniformMatrix4fv,    (GLint, GLsizei, GLboolean, const GLfloat*));

MULU_GL_DEF(void,   GenTextures,         (GLsizei, GLuint*));
MULU_GL_DEF(void,   BindTexture,         (GLenum, GLuint));
MULU_GL_DEF(void,   TexImage2D,          (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*));
MULU_GL_DEF(void,   TexParameteri,       (GLenum, GLenum, GLint));
MULU_GL_DEF(void,   TexSubImage2D,       (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*));
MULU_GL_DEF(void,   DeleteTextures,      (GLsizei, const GLuint*));
MULU_GL_DEF(void,   ActiveTexture,       (GLenum));

MULU_GL_DEF(void,   Enable,              (GLenum));
MULU_GL_DEF(void,   Disable,             (GLenum));
MULU_GL_DEF(void,   BlendFunc,           (GLenum, GLenum));
MULU_GL_DEF(void,   BlendFuncSeparate,   (GLenum, GLenum, GLenum, GLenum));
MULU_GL_DEF(void,   Viewport,            (GLint, GLint, GLsizei, GLsizei));
MULU_GL_DEF(void,   Clear,               (GLbitfield));
MULU_GL_DEF(void,   ClearColor,          (GLfloat, GLfloat, GLfloat, GLfloat));
MULU_GL_DEF(void,   Scissor,             (GLint, GLint, GLsizei, GLsizei));

MULU_GL_DEF(void,   DrawElements,        (GLenum, GLsizei, GLenum, const void*));
MULU_GL_DEF(void,   DrawArrays,          (GLenum, GLint, GLsizei));

MULU_GL_DEF(void,   PixelStorei,         (GLenum, GLint));
MULU_GL_DEF(GLenum, GetError,            (void));
MULU_GL_DEF(void,   ReadPixels,          (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*));

#undef MULU_GL_DEF

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------

bool loadGLFunctions()
{
#define LOAD(name)                                                               \
    do {                                                                         \
        name = reinterpret_cast<name##_t>(SDL_GL_GetProcAddress("gl" #name));    \
        if (!name) {                                                             \
            SDL_Log("GLFuncs: failed to load gl%s", #name);                      \
            return false;                                                        \
        }                                                                        \
    } while (0)

    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(GetShaderInfoLog);
    LOAD(DeleteShader);

    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(LinkProgram);
    LOAD(GetProgramiv);
    LOAD(GetProgramInfoLog);
    LOAD(UseProgram);
    LOAD(DeleteProgram);

    LOAD(GenVertexArrays);
    LOAD(BindVertexArray);
    LOAD(DeleteVertexArrays);

    LOAD(GenBuffers);
    LOAD(BindBuffer);
    LOAD(BufferData);
    LOAD(BufferSubData);
    LOAD(DeleteBuffers);

    LOAD(VertexAttribPointer);
    LOAD(EnableVertexAttribArray);
    LOAD(DisableVertexAttribArray);

    LOAD(GetUniformLocation);
    LOAD(Uniform1i);
    LOAD(Uniform1f);
    LOAD(Uniform2f);
    LOAD(Uniform3f);
    LOAD(Uniform4f);
    LOAD(UniformMatrix4fv);

    LOAD(GenTextures);
    LOAD(BindTexture);
    LOAD(TexImage2D);
    LOAD(TexParameteri);
    LOAD(TexSubImage2D);
    LOAD(DeleteTextures);
    LOAD(ActiveTexture);

    LOAD(Enable);
    LOAD(Disable);
    LOAD(BlendFunc);
    LOAD(BlendFuncSeparate);
    LOAD(Viewport);
    LOAD(Clear);
    LOAD(ClearColor);
    LOAD(Scissor);

    LOAD(DrawElements);
    LOAD(DrawArrays);

    LOAD(PixelStorei);
    LOAD(GetError);
    LOAD(ReadPixels);

#undef LOAD

    SDL_Log("GLFuncs: all 44 functions loaded successfully");
    return true;
}

} // namespace gl
} // namespace mulu
