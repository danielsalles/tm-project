// fx_decal.vert — TMShade ground decal: a grid mesh conforming to terrain.
// Positions are world-space (already lifted to terrain height + 0.05 on the
// CPU); UVs are rotated by m_fAngle on the CPU too. No normals needed.
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;   // unused
layout(location=2) in vec4 aColor;    // per-vertex tint (fade drives alpha)
layout(location=3) in vec2 aUV;

uniform mat4 uWorld;                  // identity (positions already world-space)

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    vec4 viewPos = uView * uWorld * vec4(aPos, 1.0);
    gl_Position = FixZ(uProj * viewPos);
}
