// sky.frag — plain texture (SELECTARG1-equivalent per audit 07 §sky; no fog, no light).
in vec2 vUV;

uniform sampler2D uTex0;

out vec4 fragColor;

void main() {
    fragColor = texture(uTex0, vUV);
}
