#include "test_framework.h"

#include "scene/CameraGestures.h"

using namespace tmx;

static const CameraGestureCfg kCfg{ false, false, 1.2f, 14.0f };

TEST(input, rotate_constants) {
    FollowCam cam{ 0.0f, 0.0f, 6.0f };
    CameraRotate(cam, 100.0f, 50.0f, kCfg);
    EXPECT_NEAR(cam.yaw, 100.0f * 0.0049f, 1e-5f);   // dx*0.0049
    EXPECT_NEAR(cam.pitch, -50.0f * 0.002f, 1e-5f);  // pitch -= dy*0.002
}

TEST(input, rotate_pitch_clamps) {
    FollowCam cam{ 0.0f, 0.0f, 6.0f };
    CameraRotate(cam, 0.0f, 10000.0f, kCfg);   // drag down hard
    EXPECT_NEAR(cam.pitch, -0.98539817f, 1e-6f);
    CameraRotate(cam, 0.0f, -10000.0f, kCfg);  // drag up hard
    EXPECT_NEAR(cam.pitch, 0.75f, 1e-6f);
}

TEST(input, rotate_yaw_wraps) {
    const float twoPi = 6.2831853f;
    FollowCam cam{ twoPi - 0.01f, 0.0f, 6.0f };
    CameraRotate(cam, 100.0f, 0.0f, kCfg);     // +0.49 -> wraps below twoPi
    EXPECT_TRUE(cam.yaw >= 0.0f && cam.yaw <= twoPi);
    EXPECT_NEAR(cam.yaw, twoPi - 0.01f + 0.49f - twoPi, 1e-4f);

    FollowCam cam2{ 0.01f, 0.0f, 6.0f };
    CameraRotate(cam2, -100.0f, 0.0f, kCfg);   // wraps above 0
    EXPECT_TRUE(cam2.yaw >= 0.0f && cam2.yaw <= twoPi);
    EXPECT_NEAR(cam2.yaw, 0.01f - 0.49f + twoPi, 1e-4f);
}

TEST(input, rotate_inverted_flips_yaw_only) {
    const CameraGestureCfg inv{ true, false, 1.2f, 14.0f };
    FollowCam cam{ 0.0f, 0.0f, 6.0f };
    CameraRotate(cam, 100.0f, 50.0f, inv);
    EXPECT_NEAR(cam.yaw, -100.0f * 0.0049f + 6.2831853f, 1e-4f); // wrapped
    EXPECT_NEAR(cam.pitch, -50.0f * 0.002f, 1e-5f);               // pitch unchanged
}

TEST(input, quarter_view_locks_rotation) {
    const CameraGestureCfg qv{ false, true, 1.2f, 14.0f };
    FollowCam cam{ 1.0f, -0.5f, 6.0f };
    CameraRotate(cam, 100.0f, 50.0f, qv);
    EXPECT_NEAR(cam.yaw, 1.0f, 1e-6f);
    EXPECT_NEAR(cam.pitch, -0.5f, 1e-6f);
}

TEST(input, zoom_fclose_floor) {
    FollowCam cam{ 0.0f, 0.0f, 6.0f };
    CameraZoom(cam, 1000.0f, kCfg); // zoom in hard
    EXPECT_NEAR(cam.dist, 1.2f, 1e-6f);

    const CameraGestureCfg mounted{ false, false, 2.5f, 14.0f };
    FollowCam cam2{ 0.0f, 0.0f, 6.0f };
    CameraZoom(cam2, 1000.0f, mounted);
    EXPECT_NEAR(cam2.dist, 2.5f, 1e-6f);
}

TEST(input, zoom_far_clamp_and_invert) {
    FollowCam cam{ 0.0f, 0.0f, 6.0f };
    CameraZoom(cam, -1000.0f, kCfg);
    EXPECT_NEAR(cam.dist, 14.0f, 1e-6f);

    const CameraGestureCfg inv{ true, false, 1.2f, 14.0f };
    FollowCam cam2{ 0.0f, 0.0f, 6.0f };
    CameraZoom(cam2, 1.0f, inv); // inverted: wheel up zooms out
    EXPECT_NEAR(cam2.dist, 7.0f, 1e-6f);
}

TEST(input, alt_drag_zoom_gesture) {
    // wheel = 3*dy (EventTranslator.cpp:240-243)
    FollowCam cam{ 0.0f, 0.0f, 6.0f };
    CameraAltDragZoom(cam, 10.0f, kCfg); // 3*10*0.05 = 1.5
    EXPECT_NEAR(cam.dist, 4.5f, 1e-5f);
}
