#version 450

// This vertex shader creates a full-screen triangle
// without needing any vertex buffers.
vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
                           vec2( 3.0, -1.0),
                           vec2(-1.0,  3.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
