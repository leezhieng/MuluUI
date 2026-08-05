// MuluUI SDL3+OpenGL Backend — Rounded-Rectangle Fragment Shader
// OpenGL 3.3 Core Profile (#version 330 core)
//
// This file is the canonical reference. The actual shader source compiled
// at runtime is embedded in GLRenderer.cpp (see kRectFragmentShader).
//
// Implements Fluent Design rounded rectangles using a signed-distance-field
// (SDF) for anti-aliased corners at any scale.

#version 330 core

in vec2 vTexCoord;   // local position within the rect: (0,0) at top-left
in vec4 vColor;      // interpolated vertex color (unused for solid fills)

uniform vec4 uRectParams; // (rectWidth, rectHeight, cornerRadius, borderWidth)
uniform vec4 uFillColor;  // (r, g, b, a)

out vec4 fragColor;

// Signed-distance to a rounded box.  p is relative to box center;
// halfSize = (w/2, h/2); r = corner radius.
float roundedBoxSDF(vec2 p, vec2 halfSize, float r) {
    vec2 d = abs(p) - halfSize + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

void main() {
    float w      = uRectParams.x;
    float h      = uRectParams.y;
    float radius = uRectParams.z;

    vec2 halfSize = vec2(w, h) * 0.5;
    vec2 local    = vTexCoord - halfSize;  // shift origin to rect center

    float dist  = roundedBoxSDF(local, halfSize, radius);
    float alpha = 1.0 - smoothstep(-1.0, 1.0, dist);

    fragColor = vec4(uFillColor.rgb, uFillColor.a * alpha);
}
