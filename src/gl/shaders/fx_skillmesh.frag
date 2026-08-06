// fx_skillmesh.frag — tex * uColor, no fog, optional alpha test.
// Blend is driven by GL state (SRC_ALPHA/ONE for EF_BRIGHT additive, or
// SRC_ALPHA/ONE_MINUS_SRC_ALPHA for EF_DEFAULT).
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex0;
uniform vec4  uColor;       // flat tint (RGBA 0..1)
uniform bool  uAlphaTest;
uniform float uAlphaRef;    // 0..255

void main() {
    vec4 c = texture(uTex0, vUV) * uColor;
    if (uAlphaTest && c.a * 255.0 < uAlphaRef)
        discard;
    fragColor = c;
}
