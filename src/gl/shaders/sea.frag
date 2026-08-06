// sea.frag — stage0 MODULATE + stage1 MODULATE2X with a constant 0.9 emissive-ish
// vertex color (TMSea non-dungeon branch: vertex diffuse is 0 and the material's
// emissive ~0.9 dominates). No fog (TMSea sets FOGENABLE 0).
in vec2 vUV0;
in vec2 vUV1;

uniform sampler2D uTex0;
uniform sampler2D uTex1;

out vec4 fragColor;

void main() {
    vec3 c = texture(uTex0, vUV0).rgb * texture(uTex1, vUV1).rgb * 2.0 * 0.9;
    fragColor = vec4(c, 1.0);
}
