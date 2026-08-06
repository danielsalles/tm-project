// fx_quad.vert — effect quad, world-space (CPU-built transform) or screen-space.
layout(location=0) in vec3 aPos;      // unit quad corner (-0.5..0.5)
layout(location=2) in vec4 aColor;    // RGBA (converted from D3D BGRA on the CPU)
layout(location=3) in vec2 aUV;

uniform mat4 uWorld;     // billboard world matrix (identity for screen-space)
uniform int uScreenSpace = 0;
uniform vec2 uScreenSize;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    if (uScreenSpace != 0) {
        // aPos.xy already in pixels relative to the quad center; uWorld carries
        // the pixel offset in its translation row.
        vec2 px = uWorld[3].xy + aPos.xy * vec2(uWorld[0][0], uWorld[1][1]);
        vec2 ndc = vec2(px.x / uScreenSize.x * 2.0 - 1.0,
                        1.0 - px.y / uScreenSize.y * 2.0);
        gl_Position = vec4(ndc, 0.0, 1.0);
    } else {
        gl_Position = FixZ(uProj * uView * uWorld * vec4(aPos, 1.0));
    }
}
