#version 450

/*
 * =========================================================
 *  Graphics Pipeline Stress Test - Fragment Shader
 *  This shader performs heavy math for every pixel,
 *  simulating a FurMark-style rendering load.
 * =========================================================
 */

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    float time;
};

// A simple, pseudo-random noise function
float noise(vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1920.0, 1080.0); // Assuming a target resolution
    vec2 c = uv * 2.0 - 1.0;

    vec2 z = c;
    float brightness = 0.0;

    // This is the "workload knob". A higher number means more math per pixel.
    // 300 is a very heavy starting point.
    const int ITERATIONS = 300;

    for (int i = 0; i < ITERATIONS; i++) {
        // Complex fractal-like math (Julia set style)
        z = vec2(z.x*z.x - z.y*z.y, 2.0*z.x*z.y) + c * sin(time * 0.1);

        // Break if the value escapes, creating a pattern
        if (dot(z, z) > 4.0) {
            break;
        }
        brightness += 1.0 / float(ITERATIONS);
    }

    // Mix with noise to prevent compiler over-optimization
    float n = noise(uv + time);
    outColor = vec4(vec3(brightness * 0.8, brightness * 0.9, brightness * 1.0) * (0.8 + n * 0.2), 1.0);
}
