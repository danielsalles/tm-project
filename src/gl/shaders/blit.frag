// blit.frag — final blit of the offscreen scene target with the brightness
// gain applied. The D3D9 gamma ramp was a pure linear gain:
//   ramp[i] = bright * 0.02 * i        (RenderDevice.cpp:288-310)
// so the faithful port is a single multiply (bright=50 -> identity).
uniform sampler2D uScene;
uniform float uBright = 1.0;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec3 c = texture(uScene, vUV).rgb;
    fragColor = vec4(c * uBright, 1.0);
}
