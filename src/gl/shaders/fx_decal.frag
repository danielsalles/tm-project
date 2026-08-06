// fx_decal.frag — tex * vColor, no fog (TMShade disables fog for EF_BRIGHT;
// EF_DEFAULT uses vertex fog but the game paths are EF_BRIGHT). Alpha test off.
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;

uniform sampler2D uTex0;

void main() {
    fragColor = texture(uTex0, vUV) * vColor;
}
