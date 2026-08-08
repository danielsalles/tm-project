#pragma once

// CameraGestures — the original camera input math, pure/testable (phase 7,
// doc 21 §3). Constants from EventTranslator.cpp:251-330:
//   rotate:  pitch -= dy*0.002 (clamp -0.98539817..0.75), yaw += dx*0.0049 (wrap 2pi)
//   zoom:    wheel (mouse) or 3*dy (Alt drag) — distance clamped [fClose, 14]
//   invert:  RenderDevice::m_bCameraRot flips dx and wheel signs
//   fClose:  1.2 (2.5 mounted) + Con*0.00019

namespace tmx {

struct FollowCam {
    float yaw = 0.0f;
    float pitch = -0.7f;
    float dist = 6.0f;
};

struct CameraGestureCfg {
    bool invert = false;      // Config[10]
    bool quarterView = false; // Config[13] — locks rotation
    float fClose = 1.2f;      // 2.5 mounted
    float fFar = 14.0f;
};

inline void CameraRotate(FollowCam& cam, float dx, float dy,
                         const CameraGestureCfg& cfg) {
    if (cfg.quarterView)
        return;
    const float sgn = cfg.invert ? -1.0f : 1.0f;
    cam.pitch -= dy * 0.002f;
    if (cam.pitch < -0.98539817f) cam.pitch = -0.98539817f;
    if (cam.pitch >  0.75f)       cam.pitch =  0.75f;
    cam.yaw += sgn * dx * 0.0049f;
    const float twoPi = 6.2831853f;
    if (cam.yaw > twoPi) cam.yaw -= twoPi;
    if (cam.yaw < 0.0f)  cam.yaw += twoPi;
}

inline void CameraZoom(FollowCam& cam, float wheel, const CameraGestureCfg& cfg) {
    const float sgn = cfg.invert ? -1.0f : 1.0f;
    cam.dist -= sgn * wheel;
    if (cam.dist < cfg.fClose) cam.dist = cfg.fClose;
    if (cam.dist > cfg.fFar)   cam.dist = cfg.fFar;
}

// Alt-drag zoom gesture: wheel = 3*dy (EventTranslator.cpp:240-243), scaled to
// distance units per pixel.
inline void CameraAltDragZoom(FollowCam& cam, float dy, const CameraGestureCfg& cfg) {
    CameraZoom(cam, 3.0f * dy * 0.05f, cfg);
}

}
