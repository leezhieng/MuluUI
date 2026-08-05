// MuluUI SDL3+OpenGL Backend — Textured-Quad Vertex Shader
// OpenGL 3.3 Core Profile (#version 330 core)
//
// This file is the canonical reference. The actual shader source compiled
// at runtime is embedded in GLRenderer.cpp (see kTexVertexShader).
//
// Purpose:
//   Transforms glyph quads (position + UV) through the orthographic
//   projection. Used by both SDF text glyphs and general textured quads.

#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform mat4 uProj;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vTexCoord   = aTexCoord;
    vColor      = aColor;
}
