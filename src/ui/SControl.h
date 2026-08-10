#pragma once

#include <cstdint>

#include "ui/UITypes.h"

namespace tmx {

// Windows message codes used by the original event dispatch
// (EventTranslator.cpp:413-461, SControl.cpp:274-329).
constexpr unsigned int WM_MOUSEMOVE      = 512;   // 0x200
constexpr unsigned int WM_LBUTTONDOWN    = 513;   // 0x201
constexpr unsigned int WM_LBUTTONUP      = 514;   // 0x202
constexpr unsigned int WM_LBUTTONDBLCLK  = 515;   // 0x203
constexpr unsigned int WM_RBUTTONDOWN    = 516;   // 0x204
constexpr unsigned int WM_RBUTTONUP      = 517;   // 0x205

// wParam bit for "left button held" (MK_LBUTTON) — checked as (wParam & 1)
// in SPanel::OnMouseEvent WM_MOUSEMOVE (SControl.cpp:295).
constexpr unsigned int MK_LBUTTON_HELD   = 1;

class UIBatcher;
class GLTextureManager;
class GLFont;

// Event listener interface — controls send events to their listener.
class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual int OnControlEvent(uint32_t controlID, uint32_t event) = 0;
};

// Base class for all UI controls. Port of SControl.h/cpp.
// Mouse coordinates arrive RELATIVE to the parent chain: the container
// subtracts parent offsets while traversing (SControlContainer.cpp:64).
class SControl {
public:
    SControl() = default;
    virtual ~SControl() = default;

    CONTROL_TYPE m_eCtrlType = CTRL_TYPE_NONE;
    uint32_t m_dwControlID = 0;
    uint32_t m_dwUniqueID = 0;

    int IsFocused() const { return m_bFocused; }

    // Position/size relative to parent (virtual 800x600 space scaled by ratio)
    float m_nPosX = 0, m_nPosY = 0;
    float m_nWidth = 0, m_nHeight = 0;

    int m_bVisible = 1;
    int m_bEnable = 1;
    int m_bFocused = 0;
    int m_bOver = 0;
    int m_bAlwaysOnTop = 0;
    int m_bModal = 0;
    int m_bDeleteThisObject = 0;
    int m_bSelectEnable = 1;

    SControl* m_pParent = nullptr;
    SControl* m_pDown = nullptr;    // first child
    SControl* m_pNext = nullptr;    // next sibling

    IEventListener* m_pEventListener = nullptr;

    virtual void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                            int layer, int arg);

    // dwFlags = WM_MOUSE* code, wParam = button bits, nX/nY relative to parent.
    virtual int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY);
    virtual int OnKeyDownEvent(int key);
    virtual int OnKeyUpEvent(int key);
    virtual int OnCharEvent(char c);

    int PtInControl(int x, int y);
    SControl* FindControl(uint32_t id);
    void AddChild(SControl* child);

    void SetControlID(uint32_t id) { m_dwControlID = id; }
    void SetEventListener(IEventListener* l) { m_pEventListener = l; }
    void SetVisible(int v) { m_bVisible = v; }

    // Centers horizontally for 5 hardcoded IDs (SControl.cpp:223-237).
    void SetCenterPos(uint32_t dwControlID, float posX, float posY, float width, float height);

    static float s_screenW;
    static float s_screenH;
    static float s_widthRatio;
    static float s_heightRatio;
};

int AddRenderControlItem(stGeomList* pDrawList, GeomControl* pGeom, int nLayer);
void RemoveRenderControlItem(stGeomList* pDrawList, GeomControl* pGeom, int nLayer);

}
