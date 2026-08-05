// MuluUI SDL3+OpenGL Backend — Rounded-Rectangle Vertex Shader
// OpenGL 3.3 Core Profile (#version 330 core)
//
// This file is the canonical reference. The actual shader source compiled
// at runtime is embedded in GLRenderer.cpp (see kRectVertexShader).
//
// Purpose:
//   Transforms 2D pixel-coordinate vertices into clip space via an
//   orthographic projection matrix. Passes local rect-coordinates through
//   vTexCoord for use by the SDF-based rounded-rect fragment shader.

#version 330 core

layout(location = 0) in vec2 aPos;       // vertex position in pixel space
layout(location = 1) in vec2 aTexCoord;  // local rect coordinate (0,0)→(w,h)
layout(location = 2) in vec4 aColor;     // per-vertex RGBA

uniform mat4 uProj;                      // orthographic projection

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vTexCoord   = aTexCoord;
    vColor      = aColor;
}
