#include "ui/SControlContainer.h"
#include "ui/SControls.h"

#include <cstring>
#include <vector>

namespace tmx {

SControlContainer::SControlContainer() {
    m_pControlRoot = new SControl();
    m_pControlRoot->m_dwControlID = 0;
    memset(m_pDrawControl, 0, sizeof(m_pDrawControl));
}

SControlContainer::~SControlContainer() {
    // Delete the tree depth-first.
    struct Node { SControl* c; };
    // Simple recursive delete via iterative stack would be overkill here;
    // controls are allocated with new and owned by their parents.
    std::vector<SControl*> stack;
    if (m_pControlRoot) stack.push_back(m_pControlRoot);
    while (!stack.empty()) {
        SControl* c = stack.back();
        stack.pop_back();
        for (SControl* child = c->m_pDown; child; child = child->m_pNext)
            stack.push_back(child);
        delete c;
    }
    m_pControlRoot = nullptr;
}

void SControlContainer::AddItem(SControl* ctrl) {
    if (m_pControlRoot)
        m_pControlRoot->AddChild(ctrl);
}

SControl* SControlContainer::FindControl(uint32_t id) {
    return m_pControlRoot ? m_pControlRoot->FindControl(id) : nullptr;
}

void SControlContainer::FrameMove(uint32_t dwServerTime) {
    m_dwServerTime = dwServerTime;

    for (int i = 0; i < MAX_DRAW_CONTROL; ++i) {
        m_pDrawControl[i].pHeadGeom = nullptr;
        m_pDrawControl[i].pTailGeom = nullptr;
    }

    if (m_bInvisibleUI || !m_pControlRoot)
        return;

    IVector2 rootPos = { 0, 0 };
    WalkTree(m_pControlRoot, rootPos, 0);

    // Cursor always on the topmost layer (SControlContainer.cpp:307-308).
    if (m_pCursor && m_pCursor->m_bVisible)
        m_pCursor->FrameMove2(m_pDrawControl, rootPos, 29, 0);
}

void SControlContainer::WalkTree(SControl* ctrl, IVector2 parentPos, int layer) {
    if (!ctrl || !ctrl->m_bVisible) return;

    IVector2 absPos = {
        parentPos.x + (int)ctrl->m_nPosX,
        parentPos.y + (int)ctrl->m_nPosY
    };

    ctrl->FrameMove2(m_pDrawControl, parentPos, layer, 0);

    int childLayer = layer + 1;
    for (SControl* child = ctrl->m_pDown; child; child = child->m_pNext)
        WalkTree(child, absPos, childLayer);
}

// Faithful port of SControlContainer.cpp:30-108 — mouse coords are converted
// to parent-relative by subtracting each ancestor's position while walking.
int SControlContainer::OnMouseEvent(unsigned int dwFlags, unsigned int wParam,
                                    int nX, int nY) {
    if (m_pCursor && m_pCursor->m_bVisible)
        m_pCursor->OnMouseEvent(dwFlags, wParam, nX, nY);

    int parentPosX = 0, parentPosY = 0;
    int bProcessed = 0;

    SControl* pCurrent = m_pControlRoot;
    SControl* pRoot = m_pControlRoot;

    // Topmost visible modal takes over the input.
    for (int i = 0; i < 8; ++i) {
        SControl* tmp = m_pModalControl[i];
        if (tmp && tmp->m_bVisible == 1 && tmp->m_bModal == 1) {
            pCurrent = tmp;
            pRoot = tmp;
            break;
        }
    }
    if (!pCurrent)
        return 1;

    do {
        if (pCurrent->m_bVisible) {
            int before = pCurrent->m_bFocused;
            int ret = pCurrent->OnMouseEvent(dwFlags, wParam,
                                             nX - parentPosX, nY - parentPosY);
            if (pCurrent->m_bFocused && !before && ret == 1 &&
                pCurrent->m_eCtrlType == CTRL_TYPE_EDITABLETEXT)
                SetFocusedControl(pCurrent);
            if (ret == 1)
                bProcessed = 1;

            if (pCurrent->m_pDown) {
                parentPosX += (int)pCurrent->m_nPosX;
                parentPosY += (int)pCurrent->m_nPosY;
                pCurrent = pCurrent->m_pDown;
                continue;
            }
        }

        do {
            if (pCurrent->m_pNext) {
                pCurrent = pCurrent->m_pNext;
                break;
            }
            pCurrent = pCurrent->m_pParent;
            if (!pCurrent)
                break;
            parentPosX -= (int)pCurrent->m_nPosX;
            parentPosY -= (int)pCurrent->m_nPosY;
        } while (pCurrent != pRoot);
    } while (pCurrent != pRoot && pCurrent != nullptr);

    return bProcessed;
}

int SControlContainer::OnKeyDownEvent(int key) {
    if (m_pFocusControl)
        return m_pFocusControl->OnKeyDownEvent(key);
    return 0;
}

int SControlContainer::OnKeyUpEvent(int key) {
    (void)key;
    return 0;
}

int SControlContainer::OnCharEvent(char c) {
    // A committed character replaces any IME composition in progress.
    OnEditingEvent(nullptr);
    if (m_pFocusControl)
        return m_pFocusControl->OnCharEvent(c);
    return 0;
}

void SControlContainer::OnEditingEvent(const char* composition) {
    auto* edit = dynamic_cast<SEditableText*>(m_pFocusControl);
    if (edit)
        edit->SetComposition(composition);
}

int SControlContainer::OnControlEvent(uint32_t controlID, uint32_t event) {
    (void)controlID; (void)event;
    return 0;
}

void SControlContainer::PushModal(SControl* ctrl) {
    for (int i = 0; i < 8; ++i) {
        if (!m_pModalControl[i]) {
            m_pModalControl[i] = ctrl;
            return;
        }
    }
}

void SControlContainer::PopModal() {
    for (int i = 7; i >= 0; --i) {
        if (m_pModalControl[i]) {
            m_pModalControl[i] = nullptr;
            return;
        }
    }
}

void SControlContainer::SetFocusedControl(SControl* ctrl) {
    if (m_pFocusControl)
        m_pFocusControl->m_bFocused = 0;
    m_pFocusControl = ctrl;
    if (m_pFocusControl)
        m_pFocusControl->m_bFocused = 1;
}

}
