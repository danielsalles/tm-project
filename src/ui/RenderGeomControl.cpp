#include "ui/RenderGeomControl.h"
#include "ui/SControls.h"

#include "gl/UIRenderer.h"
#include "gl/GLRenderDevice.h"
#include "gl/GLTexture.h"
#include "gl/GLFont.h"

#include <cstring>

namespace tmx {

bool RenderGeomControl::Init(GLRenderDevice* dev, GLTextureManager* tex,
                              UIRenderer* ui, GLFont* font, std::string* err) {
    m_dev = dev;
    m_tex = tex;
    m_ui = ui;
    m_font = font;
    return true;
}

void RenderGeomControl::Shutdown() {
}

void RenderGeomControl::Render(GeomControl* ctrl) {
    if (!ctrl || !m_ui) return;

    switch (ctrl->eRenderType) {
        case RENDER_TEXT:
        case RENDER_SHADOW:
            // Text rendering via GLFont — strColor, not dwColor (buttons tint
            // the image rect but keep white labels, SControl.cpp:1412).
            if (m_font && ctrl->strString[0]) {
                m_font->SetText(ctrl->strString, ctrl->strColor);
                m_font->Render(m_ui->Batch(), ctrl->nPosX, ctrl->nPosY,
                               ctrl->eRenderType == RENDER_SHADOW ? 1 : 0,
                               ctrl->nLayer);
            }
            break;

        case RENDER_IMAGE:
        case RENDER_IMAGE_TILE:
        case RENDER_IMAGE_STRETCH:
        case RENDER_TEXT_FOCUS:
            RenderGeomRectImage(ctrl);
            // If there's text overlay, render it centered
            if (ctrl->strString[0] && m_font) {
                m_font->SetText(ctrl->strString, ctrl->strColor);
                float textX = ctrl->nPosX + ctrl->nWidth * 0.5f - m_font->GetLastWidth() * 0.5f;
                float textY = ctrl->nPosY + ctrl->nHeight * 0.5f - m_font->GetLastHeight() * 0.5f;
                m_font->Render(m_ui->Batch(), textX, textY, 0, ctrl->nLayer);
            }
            break;

        case RENDER_3DOBJ:
            // 3D objects in UI — deferred to Phase 8
            break;

        default:
            break;
    }

    // Guild mark overlay (RenderDevice.cpp:3160-3230): 16x12 BMP drawn over
    // the panel corner; gold/silver backing rect for layouts 1/2.
    if (ctrl->nMarkIndex >= 0 && ctrl->nMarkIndex < GLTextureManager::GUILD_MARK_COUNT &&
        m_tex && m_ui) {
        GLuint mark = m_tex->GetGuildMarkTexture(ctrl->nMarkIndex);
        if (mark) {
            const float iX = ctrl->nPosX - 2.0f;
            const float iY = ctrl->nPosY - 2.0f;
            if (ctrl->nMarkLayout == 1)
                m_ui->RenderRectNoTex(iX, iY, 20.0f, 16.0f, 0xFFFFD700, true);
            else if (ctrl->nMarkLayout == 2)
                m_ui->RenderRectNoTex(iX, iY, 20.0f, 16.0f, 0xFFC0C0C0, true);
            m_ui->RenderRect(0.0f, 0.0f, 16.0f, 12.0f, iX, iY, mark, 16, 12,
                             1.0f, 1.0f);
        }
    }
}

void RenderGeomControl::RenderGeomRectImage(GeomControl* ctrl) {
    if (!ctrl || !m_tex || !m_ui) return;

    // Texture set resolution (RenderDevice.cpp:2833,3129-3160,3236-3244):
    //   n >= 0  → GetUITextureSet(n)
    //   n == -1 → solid color rect (bTrans=0)
    //   n == -2 → blended color rect (bTrans=1); text-only controls tint ~invisible
    //   n < -2  → GetUITextureSet(-n)  (login/logo panels use this)
    if (ctrl->nTextureSetIndex == -1 || ctrl->nTextureSetIndex == -2) {
        m_ui->RenderRectNoTex(ctrl->nPosX, ctrl->nPosY, ctrl->nWidth, ctrl->nHeight,
                              ctrl->dwColor, ctrl->nTextureSetIndex == -2);
        return;
    }

    const int setIndex = ResolveTextureSetIndex(ctrl->nTextureSetIndex);
    auto* uiSet = m_tex->GetUITextureSet(setIndex);
    if (!uiSet || ctrl->nTextureIndex < 0 ||
        ctrl->nTextureIndex >= (int)uiSet->coords.size()) {
        // Set/index missing — draw nothing (the original would render garbage;
        // transparent is the safe equivalent)
        return;
    }

    auto& coord = uiSet->coords[ctrl->nTextureIndex];
    GLuint texture = m_tex->GetUITexture(coord.nTextureIndex, 2000);
    int texW = 0, texH = 0;
    m_tex->GetUITextureSize(coord.nTextureIndex, &texW, &texH);

    switch (ctrl->eRenderType) {
        case RENDER_IMAGE:
            m_ui->RenderRect((float)coord.nStartX, (float)coord.nStartY,
                             (float)coord.nWidth, (float)coord.nHeight,
                             coord.nDestX + ctrl->nPosX, coord.nDestY + ctrl->nPosY,
                             texture, texW, texH, 1.0f, 1.0f);
            break;

        case RENDER_IMAGE_STRETCH:
        case RENDER_TEXT_FOCUS: {
            float scaleX = coord.nWidth > 0 ? ctrl->nWidth / (float)coord.nWidth : 1.0f;
            float scaleY = coord.nHeight > 0 ? ctrl->nHeight / (float)coord.nHeight : 1.0f;
            m_ui->RenderRectC((float)coord.nStartX, (float)coord.nStartY,
                              (float)coord.nWidth, (float)coord.nHeight,
                              coord.nDestX + ctrl->nPosX, coord.nDestY + ctrl->nPosY,
                              texture, texW, texH, ctrl->dwColor, scaleX, scaleY);

            // Sanctum/legend overlays (texture set 338 in the original)
            if (ctrl->sSanc > 0 || ctrl->sLegend > 0) {
                GLuint glowTex = m_tex->GetUITexture(338, 2000);
                int gw = 0, gh = 0;
                m_tex->GetUITextureSize(338, &gw, &gh);
                if (glowTex) {
                    m_ui->RenderRectC(0, 0, (float)gw, (float)gh,
                                      ctrl->nPosX, ctrl->nPosY,
                                      glowTex, gw, gh, 0xFFFFFFFF,
                                      ctrl->nWidth / (gw > 0 ? gw : 1),
                                      ctrl->nHeight / (gh > 0 ? gh : 1));
                }
            }
            break;
        }

        case RENDER_IMAGE_TILE:
            m_ui->RenderRectCoord(ctrl->nPosX, ctrl->nPosY, ctrl->nWidth, ctrl->nHeight,
                                  texture, ctrl->dwColor, 0, 0);
            break;

        default:
            break;
    }
}

void RenderGeomControl::RenderLayer(stGeomList* list) {
    if (!list) return;
    for (GeomControl* ctrl = list->pHeadGeom; ctrl; ctrl = ctrl->m_pNextGeom) {
        Render(ctrl);
    }
}

void RenderGeomControl::RenderAll(stGeomList drawLists[MAX_DRAW_CONTROL]) {
    for (int i = 0; i < MAX_DRAW_CONTROL; ++i) {
        RenderLayer(&drawLists[i]);
    }
}

}
