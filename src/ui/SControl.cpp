#include "ui/SControl.h"

#include <cstring>
#include <cmath>

namespace tmx {

float SControl::s_screenW = 800.0f;
float SControl::s_screenH = 600.0f;
float SControl::s_widthRatio = 1.0f;
float SControl::s_heightRatio = 1.0f;

void SControl::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                           int layer, int arg) {
    (void)pDrawList; (void)parentPos; (void)layer; (void)arg;
}

int SControl::OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) {
    (void)dwFlags; (void)wParam; (void)nX; (void)nY;
    return 0;
}

int SControl::OnKeyDownEvent(int key) { (void)key; return 0; }
int SControl::OnKeyUpEvent(int key) { (void)key; return 0; }
int SControl::OnCharEvent(char c) { (void)c; return 0; }

int SControl::PtInControl(int x, int y) {
    return PointInRect((float)x, (float)y, m_nPosX, m_nPosY, m_nWidth, m_nHeight);
}

SControl* SControl::FindControl(uint32_t id) {
    if (m_dwControlID == id)
        return this;
    for (SControl* child = m_pDown; child; child = child->m_pNext) {
        SControl* found = child->FindControl(id);
        if (found) return found;
    }
    return nullptr;
}

void SControl::AddChild(SControl* child) {
    if (!child) return;
    child->m_pParent = this;
    child->m_pNext = nullptr;
    if (!m_pDown) {
        m_pDown = child;
    } else {
        SControl* last = m_pDown;
        while (last->m_pNext) last = last->m_pNext;
        last->m_pNext = child;
    }
}

void SControl::SetCenterPos(uint32_t dwControlID, float posX, float posY,
                            float width, float height) {
    (void)posX; (void)posY; (void)height;
    // Hardcoded IDs centered horizontally (SControl.cpp:223-237).
    static const uint32_t kCenterUI[5] = { 769, 4622, 65870, 4617, 5638 };
    for (int i = 0; i < 5; ++i) {
        if (dwControlID == kCenterUI[i]) {
            m_nPosX = (s_screenW * 0.5f) - width * 0.5f;
            return;
        }
    }
}

int AddRenderControlItem(stGeomList* pDrawList, GeomControl* pGeom, int nLayer) {
    if (nLayer < 0 || nLayer >= MAX_DRAW_CONTROL || !pGeom)
        return 0;
    pGeom->nLayer = nLayer;
    pGeom->m_pNextGeom = nullptr;
    if (pDrawList[nLayer].pHeadGeom) {
        pDrawList[nLayer].pTailGeom->m_pNextGeom = pGeom;
    } else {
        pDrawList[nLayer].pHeadGeom = pGeom;
    }
    pDrawList[nLayer].pTailGeom = pGeom;
    return 1;
}

void RemoveRenderControlItem(stGeomList* pDrawList, GeomControl* pGeom, int nLayer) {
    if (nLayer < 0 || nLayer >= MAX_DRAW_CONTROL || !pGeom)
        return;
    GeomControl* prev = nullptr;
    for (GeomControl* cur = pDrawList[nLayer].pHeadGeom; cur; cur = cur->m_pNextGeom) {
        if (cur == pGeom) {
            if (prev) prev->m_pNextGeom = cur->m_pNextGeom;
            else pDrawList[nLayer].pHeadGeom = cur->m_pNextGeom;
            if (pDrawList[nLayer].pTailGeom == cur)
                pDrawList[nLayer].pTailGeom = prev;
            cur->m_pNextGeom = nullptr;
            return;
        }
        prev = cur;
    }
}

}
