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
            // Text rendering via GLFont
            if (m_font && ctrl->strString[0]) {
                m_font->SetText(ctrl->strString, ctrl->dwColor);
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
                m_font->SetText(ctrl->strString, ctrl->dwColor);
                float textX = ctrl->nPosX + ctrl->nWidth * 0.5f - m_font->GetLastWidth() * 0.5f;
                float textY = ctrl->nPosY + ctrl->nHeight * 0.5f - m_font->GetLastHeight() * 0.5f;
                m_font->Render(m_ui->Batch(), textX, textY, 0, ctrl->nLayer);
            }
            break;

        case RENDER_3DOBJ:
            // 3D objects in UI — deferred to Phase 7
            break;

        default:
            break;
    }
}

void RenderGeomControl::RenderGeomRectImage(GeomControl* ctrl) {
    if (!ctrl || !m_tex || !m_ui) return;

    // Solid color rectangle (nTextureSetIndex == -1)
    if (ctrl->nTextureSetIndex == -1) {
        m_ui->RenderRectNoTex(ctrl->nPosX, ctrl->nPosY, ctrl->nWidth, ctrl->nHeight,
                              ctrl->dwColor, true);
        return;
    }

    // Get the ControlTextureSet for this control
    auto* uiSet = m_tex->GetUITextureSet(ctrl->nTextureSetIndex);
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
