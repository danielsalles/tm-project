// mesh_lit.frag — combiner #1 only (05 §5.4): c = tex * vColor, then linear fog.
// Alpha test replicates D3DRS_ALPHATESTENABLE/ALPHAFUNC=GREATER/ALPHAREF.
in vec2 vUV;
in vec4 vColor;
in float vFogDepth;
out vec4 fragColor;

uniform sampler2D uTex0;
uniform float uAlphaRef;   // 0..255, same scale as D3DRS_ALPHAREF
uniform bool  uAlphaTest;

void main() {
    vec4 c = texture(uTex0, vUV) * vColor;
    if (uAlphaTest && c.a * 255.0 < uAlphaRef)
        discard;
    c.rgb = mix(uFogColor.rgb, c.rgb, FogFactor(vFogDepth));
    fragColor = c;
}
