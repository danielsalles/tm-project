#pragma once

#include <cstdint>

#include "gl/UIBatcher.h"
#include "math/TMMath.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;

// UI rendering primitives — mapping of the original RenderDevice's RenderRect*
// functions. Source rect (startX/startY/cx/cy) is in TEXTURE PIXELS and gets
// normalized by texW/texH (the original queries the D3D texture desc; we pass
// the cached size explicitly). The resolved GL texture rides in texHandle.
class UIRenderer {
public:
    bool Init(GLuint whiteTex, std::string* err);
    void Destroy();

    void RenderRectC(float startX, float startY, float cx, float cy,
                     float destX, float destY, GLuint texture,
                     int texW, int texH, uint32_t color,
                     float scaleX = 1.0f, float scaleY = 1.0f);

    void RenderRect(float startX, float startY, float cx, float cy,
                    float destX, float destY, GLuint texture,
                    int texW, int texH,
                    float scaleX = 1.0f, float scaleY = 1.0f);

    void RenderRectTex(float startX, float startY, float cx, float cy,
                       float destX, float destY, float destCX, float destCY,
                       GLuint texture, int texW, int texH,
                       uint32_t color, bool transparent,
                       float angle = 0.0f, float scale = 1.0f);

    void RenderRectNoTex(float x, float y, float cx, float cy,
                         uint32_t color, bool transparent);

    void RenderRectCoord(float destX, float destY, float cx, float cy,
                         GLuint texture, uint32_t color,
                         float fU, float fV);

    void RenderRectTexDamage(float startX, float startY, float cx, float cy,
                             float destX, float destY, float destCX, float destCY,
                             GLuint texture, int texW, int texH,
                             uint32_t color, bool transparent,
                             float angle = 0.0f, float scale = 1.0f);

    void RenderRectRot(float startX, float startY, float cx, float cy,
                       float destX, float destY, float cenX, float cenY,
                       float angle, GLuint texture, int texW, int texH,
                       float scaleX = 1.0f, float scaleY = 1.0f);

    void RenderRectProgress2(float x, float y, float cx, float cy,
                             float progress, uint32_t color);

    UIBatcher& Batch() { return m_batch; }
    const UIBatcher& Batch() const { return m_batch; }

    void Begin() { m_batch.Begin(); }
    void Flush(GLRenderDevice& dev, GLTextureManager& tex, int screenW, int screenH) {
        m_batch.Flush(dev, tex, screenW, screenH);
    }

    int currentLayer = 0;

private:
    UIBatcher m_batch;
    GLuint m_whiteTex = 0;
};

}
