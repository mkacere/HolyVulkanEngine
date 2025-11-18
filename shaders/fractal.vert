#version 450

// Fullscreen triangle vertex shader
// No vertex buffers required - generates procedural triangle that covers the screen

void main() {
    // Generate fullscreen triangle:
    // Vertex 0: (-1, -1) - bottom-left
    // Vertex 1: ( 3, -1) - extends beyond right edge
    // Vertex 2: (-1,  3) - extends beyond top edge
    // This creates a triangle that covers the entire screen
    const vec2 pos[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
}
