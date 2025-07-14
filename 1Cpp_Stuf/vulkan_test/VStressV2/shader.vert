#version 450

// We now pass the color down to the fragment shader
layout (location = 0) out vec4 outColor;

vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
                           vec2( 3.0, -1.0),
                           vec2(-1.0,  3.0)
);

// We will draw 8 layers for massive overdraw
const int LAYER_COUNT = 8;

void main() {
    // Calculate a small offset based on the instance index
    float layer = float(gl_InstanceIndex);
    float offset = layer * 0.01;

    gl_Position = vec4(positions[gl_VertexIndex] + offset, 0.0, 1.0);

    // Assign a slightly different color to each layer
    outColor = vec4(0.1, 0.2, 0.3, 0.1); // Low alpha for blending
}
