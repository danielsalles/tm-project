// fx_skillmesh.vert — TMEffectMesh render path: a common mesh drawn as a skill
// VFX with a flat tint, no lighting, no fog (TMEffectMesh::Render disables both).
// Reuses the mesh VAO layout (pos/normal/color/uv at locations 0-3) but ignores
// normals; the tint comes from the uColor uniform (the original locks the VB and
// overwrites every vertex's diffuse with the tint color).
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;   // unused
layout(location=2) in vec4 aColor;    // unused (tint is uniform)
layout(location=3) in vec2 aUV;

uniform mat4 uWorld;

out vec2 vUV;

void main() {
    vUV = aUV;
    vec4 viewPos = uView * uWorld * vec4(aPos, 1.0);
    gl_Position = FixZ(uProj * viewPos);
}
