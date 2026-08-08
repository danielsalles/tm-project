#include "gl/UIRenderer.h"

#include "gl/GLRenderDevice.h"
#include "gl/GLTexture.h"

#include <cmath>

namespace tmx {

bool UIRenderer::Init(GLuint whiteTex, std::string* err) {
    m_whiteTex = whiteTex;
    if (!m_batch.Init(whiteTex, err))
        return false;
    return true;
}

void UIRenderer::Destroy() {
    m_batch.Destroy();
}

// Normalizes a texture-pixel rect into UVs.
static inline void NormUV(float startX, float startY, float cx, float cy,
                          int texW, int texH, float outUV[4]) {
    float tw = texW > 0 ? (float)texW : 1.0f;
    float th = texH > 0 ? (float)texH : 1.0f;
    outUV[0] = startX / tw;
    outUV[1] = startY / th;
    outUV[2] = (startX + cx) / tw;
    outUV[3] = (startY + cy) / th;
}

void UIRenderer::RenderRectC(float startX, float startY, float cx, float cy,
                              float destX, float destY, GLuint texture,
                              int texW, int texH, uint32_t color,
                              float scaleX, float scaleY) {
    UIQuad q;
    q.x = destX; q.y = destY;
    q.w = cx * scaleX; q.h = cy * scaleY;
    float uv[4]; NormUV(startX, startY, cx, cy, texW, texH, uv);
    q.u0 = uv[0]; q.v0 = uv[1]; q.u1 = uv[2]; q.v1 = uv[3];
    q.color = color;
    q.texIndex = -1;
    q.texHandle = texture;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRect(float startX, float startY, float cx, float cy,
                             float destX, float destY, GLuint texture,
                             int texW, int texH,
                             float scaleX, float scaleY) {
    UIQuad q;
    q.x = destX; q.y = destY;
    q.w = cx * scaleX; q.h = cy * scaleY;
    float uv[4]; NormUV(startX, startY, cx, cy, texW, texH, uv);
    q.u0 = uv[0]; q.v0 = uv[1]; q.u1 = uv[2]; q.v1 = uv[3];
    q.color = 0xFFFFFFFF;
    q.texIndex = -1;
    q.texHandle = texture;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRectTex(float startX, float startY, float cx, float cy,
                                float destX, float destY, float destCX, float destCY,
                                GLuint texture, int texW, int texH,
                                uint32_t color, bool transparent,
                                float angle, float scale) {
    (void)transparent; (void)angle;
    UIQuad q;
    q.x = destX; q.y = destY;
    q.w = destCX * scale; q.h = destCY * scale;
    float uv[4]; NormUV(startX, startY, cx, cy, texW, texH, uv);
    q.u0 = uv[0]; q.v0 = uv[1]; q.u1 = uv[2]; q.v1 = uv[3];
    q.color = color;
    q.texIndex = -1;
    q.texHandle = texture;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRectNoTex(float x, float y, float cx, float cy,
                                  uint32_t color, bool transparent) {
    (void)transparent;
    UIQuad q;
    q.x = x; q.y = y; q.w = cx; q.h = cy;
    q.u0 = 0; q.v0 = 0; q.u1 = 1; q.v1 = 1;
    q.color = color;
    q.texIndex = -1;
    q.texHandle = m_whiteTex;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRectCoord(float destX, float destY, float cx, float cy,
                                  GLuint texture, uint32_t color,
                                  float fU, float fV) {
    (void)cx; (void)cy;
    UIQuad q;
    q.x = destX; q.y = destY; q.w = cx; q.h = cy;
    q.u0 = fU; q.v0 = fV;
    q.u1 = fU + 1.0f; q.v1 = fV + 1.0f;
    q.color = color;
    q.texIndex = -1;
    q.texHandle = texture;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRectTexDamage(float startX, float startY, float cx, float cy,
                                      float destX, float destY, float destCX, float destCY,
                                      GLuint texture, int texW, int texH,
                                      uint32_t color, bool transparent,
                                      float angle, float scale) {
    (void)transparent; (void)angle;
    UIQuad q;
    q.x = destX - destCX * 0.5f;
    q.y = destY - destCY * 0.5f;
    q.w = destCX * scale;
    q.h = destCY * scale;
    float uv[4]; NormUV(startX, startY, cx, cy, texW, texH, uv);
    q.u0 = uv[0]; q.v0 = uv[1]; q.u1 = uv[2]; q.v1 = uv[3];
    q.color = color;
    q.texIndex = -1;
    q.texHandle = texture;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRectRot(float startX, float startY, float cx, float cy,
                                float destX, float destY, float cenX, float cenY,
                                float angle, GLuint texture, int texW, int texH,
                                float scaleX, float scaleY) {
    (void)cenX; (void)cenY; (void)angle;
    UIQuad q;
    q.x = destX; q.y = destY;
    q.w = cx * scaleX; q.h = cy * scaleY;
    float uv[4]; NormUV(startX, startY, cx, cy, texW, texH, uv);
    q.u0 = uv[0]; q.v0 = uv[1]; q.u1 = uv[2]; q.v1 = uv[3];
    q.color = 0xFFFFFFFF;
    q.texIndex = -1;
    q.texHandle = texture;
    q.layer = currentLayer;
    m_batch.Push(q);
}

void UIRenderer::RenderRectProgress2(float x, float y, float cx, float cy,
                                      float progress, uint32_t color) {
    if (progress <= 0.0f) return;
    if (progress > 1.0f) progress = 1.0f;
    UIQuad q;
    q.x = x; q.y = y;
    q.w = cx * progress; q.h = cy;
    q.u0 = 0; q.v0 = 0; q.u1 = progress; q.v1 = 1;
    q.color = color;
    q.texIndex = -1;
    q.texHandle = m_whiteTex;
    q.layer = currentLayer;
    m_batch.Push(q);
}

}
