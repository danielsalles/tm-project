#pragma once

#include <cstdint>

namespace tmx {

// Simple 2D integer vector for UI positioning.
struct IVector2 {
    int x = 0;
    int y = 0;
    IVector2() = default;
    IVector2(int ix, int iy) : x(ix), y(iy) {}
};

// UI control types and render types — mirrors the original Enums.h + SControl.h.
// Kept in a separate header to avoid pulling in the full SControl dependency chain.

enum CONTROL_TYPE {
    CTRL_TYPE_NONE = -1,
    CTRL_TYPE_CURSOR = 0,
    CTRL_TYPE_PANEL = 1,
    CTRL_TYPE_BUTTON = 2,
    CTRL_TYPE_CHECKBOX = 3,
    CTRL_TYPE_RADIOBUTTON = 4,
    CTRL_TYPE_RADIOBUTTONSET = 5,
    CTRL_TYPE_LISTBOX = 6,
    CTRL_TYPE_LISTBOXITEM = 7,
    CTRL_TYPE_MESSAGEBOX = 8,
    CTRL_TYPE_MESSAGEPANEL = 9,
    CTRL_TYPE_PROGRESSBAR = 10,
    CTRL_TYPE_SCROLLBAR = 11,
    CTRL_TYPE_TEXT = 12,
    CTRL_TYPE_EDITABLETEXT = 13,
    CTRL_TYPE_DIALOG = 14,
    CTRL_TYPE_3DOBJ = 15,
    CTRL_TYPE_GRID = 16,
};

enum RENDERCTRLTYPE {
    RENDER_NONE = -1,
    RENDER_TEXT = 0,
    RENDER_SHADOW = 1,
    RENDER_IMAGE = 2,
    RENDER_IMAGE_TILE = 3,
    RENDER_IMAGE_STRETCH = 4,
    RENDER_3DOBJ = 5,
    RENDER_TEXT_FOCUS = 6,
};

// GeomControl — the atomic unit of UI rendering.
// Accumulated during FrameMove2, flushed by UIBatcher at end of frame.
struct GeomControl {
    RENDERCTRLTYPE eRenderType = RENDER_NONE;
    float nPosX = 0, nPosY = 0;
    float nWidth = 0, nHeight = 0;
    uint32_t dwColor = 0xFFFFFFFF;
    int nTextureSetIndex = -1;
    int nTextureIndex = 0;
    int nLayer = 0;
    void* pFont = nullptr;    // TMFont2* or GLFont*
    char strString[256] = {};
    float fAngle = 0;
    float fScale = 1.0f;
    int n3DObjIndex = 0;
    short sLegend = 0;
    short sSanc = 0;
    int nMarkIndex = -1;
    uint32_t dwBGColor = 0;
    int bClip = 0;
    GeomControl* m_pNextGeom = nullptr;  // linked list within a layer
};

// Linked list of GeomControls per draw layer.
struct stGeomList {
    GeomControl* pHeadGeom = nullptr;
    GeomControl* pTailGeom = nullptr;
};

constexpr int MAX_DRAW_CONTROL = 30;

// Utility: point-in-rect test.
inline int PointInRect(float px, float py, float rx, float ry, float rw, float rh) {
    return px >= rx && py >= ry && (rx + rw) > px && (ry + rh) > py;
}

}
