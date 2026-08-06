// sky.vert — unlit dome, follows the camera (world built per frame in C++).
layout(location=0) in vec3 aPos;
layout(location=3) in vec2 aUV;

uniform mat4 uWorld;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = FixZ(uProj * uView * uWorld * vec4(aPos, 1.0));
}
