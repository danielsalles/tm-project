// sea.vert — procedural sea grid; positions are local, uWorld places the plane.
layout(location=0) in vec3 aPos;
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;

uniform mat4 uWorld;

out vec2 vUV0;
out vec2 vUV1;

void main() {
    vUV0 = aUV0;
    vUV1 = aUV1;
    gl_Position = FixZ(uProj * uView * uWorld * vec4(aPos, 1.0));
}
