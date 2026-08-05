// MuluUI SDL3+OpenGL Backend — Textured-Quad Fragment Shader
// OpenGL 3.3 Core Profile (#version 330 core)
//
// This file is the canonical reference. The actual shader source compiled
// at runtime is embedded in GLRenderer.cpp (see kTexFragmentShader).
//
// Renders glyphs from a single-channel (R8) grayscale atlas texture.
// Glyphs are rasterised by stbtt_MakeGlyphBitmap, producing standard
// coverage-based grayscale (0 = transparent, 255 = opaque).

#version 330 core

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;  // glyph atlas (R8 grayscale)

out vec4 fragColor;

void main() {
    // Direct grayscale alpha — the atlas stores coverage values,
    // not signed distance fields.
    float alpha = texture(uTexture, vTexCoord).r;

    // Apply the glyph color with the sampled alpha.
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
