#version 450

layout (location = 0) in vec4 inColor;
layout (location = 0) out vec4 outColor;

// We now take a texture sampler as input
layout (binding = 0) uniform sampler2D noiseTexture;

layout(push_constant) uniform PushConstants {
    float time;
};

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1920.0, 1080.0);
    vec2 c = uv * 2.0 - 1.0;

    vec2 z = c;
    float brightness = 0.0;

    // Increased iterations for even more math
    const int ITERATIONS = 400;

    for (int i = 0; i < ITERATIONS; i++) {
        // Read from the texture on EVERY iteration. This thrashes the cache.
        vec4 noise = texture(noiseTexture, uv + z * 0.1);

        // Mix the texture read into the calculation
        z = vec2(z.x*z.x - z.y*z.y, 2.0*z.x*z.y) + c * sin(time * 0.1 + noise.r);

        if (dot(z, z) > 4.0) break;
        brightness += 1.0 / float(ITERATIONS);
    }

    // Output a blend of the fractal and the input layer color
    outColor = vec4(vec3(brightness), 1.0) * inColor;
}
