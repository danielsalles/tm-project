#pragma once

#include "ui/SControl.h"
#include "ui/UITypes.h"

namespace tmx {

class SCursor;

// Root manager for the UI control tree. Port of SControlContainer.
// Owns the tree, dispatches input (parent-relative coords), fills draw lists.
class SControlContainer : public IEventListener {
public:
    SControlContainer();
    ~SControlContainer();

    SControl* GetRoot() { return m_pControlRoot; }
    SControl* FindControl(uint32_t id);
    void AddItem(SControl* ctrl);  // adds to root

    // Per-frame: walk tree, generate GeomControls in draw lists.
    void FrameMove(uint32_t dwServerTime);

    // Input dispatch (returns 1 if consumed). nX/nY in absolute screen pixels.
    int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY);
    int OnKeyDownEvent(int key);
    int OnKeyUpEvent(int key);
    int OnCharEvent(char c);
    // IME composition update (SDL_EVENT_TEXT_EDITING). nullptr clears.
    void OnEditingEvent(const char* composition);

    int OnControlEvent(uint32_t controlID, uint32_t event) override;

    void PushModal(SControl* ctrl);
    void PopModal();

    void SetFocusedControl(SControl* ctrl);
    SControl* GetFocusControl() { return m_pFocusControl; }

    void SetCursor(SCursor* cursor) { m_pCursor = cursor; }

    void SetInvisibleUI(int inv) { m_bInvisibleUI = inv; }

    stGeomList m_pDrawControl[MAX_DRAW_CONTROL];

    SControl* m_pPickedControl = nullptr;  // control being dragged

private:
    void WalkTree(SControl* ctrl, IVector2 parentPos, int layer);

    SControl* m_pControlRoot = nullptr;
    SCursor* m_pCursor = nullptr;
    SControl* m_pFocusControl = nullptr;
    SControl* m_pModalControl[8] = {};
    int m_bInvisibleUI = 0;
    uint32_t m_dwServerTime = 0;
};

}
