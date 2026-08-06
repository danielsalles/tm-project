// fx_quad.frag — modulated texture (D3DTSS MODULATE); blend comes from GL state.
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTex0;

out vec4 fragColor;

void main() {
    fragColor = texture(uTex0, vUV) * vColor;
}
