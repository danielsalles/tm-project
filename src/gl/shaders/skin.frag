// skin.frag — same combiner as mesh_lit (MODULATE tex x lit color) + alpha test
// (TMTree uses ALPHAREF 0xAA) + linear fog.
in vec2 vUV;
in vec4 vColor;
in float vFogDepth;
out vec4 fragColor;

uniform sampler2D uTex0;
uniform float uAlphaRef;
uniform bool  uAlphaTest;
uniform float uAlphaMul;   // per-instance material alpha (leaf distance fade)
uniform vec3  uEmissiveAdd; // mouse-over highlight (emissive swap)

void main() {
    vec4 c = texture(uTex0, vUV) * vColor;
    c.rgb += uEmissiveAdd;
    c.a *= uAlphaMul;
    if (uAlphaTest && c.a * 255.0 < uAlphaRef)
        discard;
    c.rgb = mix(uFogColor.rgb, c.rgb, FogFactor(vFogDepth));
    fragColor = c;
}
