#pragma once

#include "ui/UITypes.h"

#include <string>

namespace tmx {

class UIRenderer;
class GLTextureManager;
class GLFont;
class GLRenderDevice;

// Central UI dispatch — routes each GeomControl to the appropriate renderer.
// Port of RenderDevice::RenderGeomControl + RenderGeomRectImage.
class RenderGeomControl {
public:
    bool Init(GLRenderDevice* dev, GLTextureManager* tex, UIRenderer* ui,
              GLFont* font, std::string* err);
    void Shutdown();

    // Render a single GeomControl.
    void Render(GeomControl* ctrl);

    // Texture set index resolution (RenderDevice.cpp:2833,3129):
    // n >= 0 → set n; n < -2 → set (-n); -1/-2 stay as-is (color-rect path).
    static int ResolveTextureSetIndex(int nTextureSetIndex) {
        return nTextureSetIndex < -2 ? -nTextureSetIndex : nTextureSetIndex;
    }

    // Render all controls in a draw list (called per layer).
    void RenderLayer(stGeomList* list);

    // Render all 30 layers (called once per frame after FrameMove).
    void RenderAll(stGeomList drawLists[MAX_DRAW_CONTROL]);

    // Set screen dimensions for UV normalization.
    void SetScreenSize(int w, int h) { m_screenW = w; m_screenH = h; }

private:
    void RenderGeomRectImage(GeomControl* ctrl);

    GLRenderDevice* m_dev = nullptr;
    GLTextureManager* m_tex = nullptr;
    UIRenderer* m_ui = nullptr;
    GLFont* m_font = nullptr;
    int m_screenW = 800;
    int m_screenH = 600;
};

}
