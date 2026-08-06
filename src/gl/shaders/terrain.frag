// terrain.frag — stage0 MODULATE (tex0 x vColor); stage1 either DISABLE (normal tiles)
// or MODULATE2X (lava 38/39, water 62-65 — TMGround.cpp:3086-3121).
in vec2 vUV0;
in vec2 vUV1;
in vec4 vColor;
in float vFogDepth;

uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform int uModulate2X;   // per-batch: 0 normal, 1 lava/water

out vec4 fragColor;

void main() {
    vec4 t0 = texture(uTex0, vUV0);
    vec4 c = t0 * vColor;
    if (uModulate2X != 0)
        c.rgb = c.rgb * texture(uTex1, vUV1).rgb * 2.0;
    c.rgb = mix(uFogColor.rgb, c.rgb, FogFactor(vFogDepth));
    fragColor = vec4(c.rgb, t0.a * vColor.a);
}
