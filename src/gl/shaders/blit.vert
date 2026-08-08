// blit.vert — fullscreen triangle from gl_VertexID (no vertex buffers).
// Covers the whole viewport; UVs run bottom-up to match the FBO texture.
out vec2 vUV;
void main() {
    // ID 0 -> (-1,-1), 1 -> (3,-1), 2 -> (-1,3): one triangle over the screen.
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                  (gl_VertexID == 2) ? 3.0 : -1.0);
    vUV = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
