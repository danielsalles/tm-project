#include "ui/SControls.h"

#include <cstring>
#include <cmath>

namespace tmx {

// ---- SPanel ----

SPanel::SPanel() {
    m_eCtrlType = CTRL_TYPE_PANEL;
    memset(&m_GCPanel, 0, sizeof(m_GCPanel));
    m_GCPanel.eRenderType = RENDER_IMAGE_STRETCH;
}

SPanel::SPanel(int textureSetIndex, float x, float y, float w, float h,
               uint32_t color, int renderType)
    : SPanel()
{
    m_nPosX = x; m_nPosY = y;
    m_nWidth = w; m_nHeight = h;
    m_GCPanel.nTextureSetIndex = textureSetIndex;
    m_GCPanel.dwColor = color;
    m_GCPanel.eRenderType = (RENDERCTRLTYPE)renderType;
    m_GCPanel.nWidth = w;
    m_GCPanel.nHeight = h;
}

void SPanel::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                          int layer, int arg) {
    (void)arg;
    if (!m_bVisible) return;
    if (m_GCPanel.nTextureSetIndex == 0 && (m_GCPanel.dwColor & 0xFF000000) == 0)
        return;

    m_GCPanel.nPosX = parentPos.x + m_nPosX;
    m_GCPanel.nPosY = parentPos.y + m_nPosY;

    float vw = SControl::s_screenW;
    float vh = SControl::s_screenH;
    if (m_GCPanel.nPosX + m_nWidth < 0 || m_GCPanel.nPosX > vw ||
        m_GCPanel.nPosY + m_nHeight < 0 || m_GCPanel.nPosY > vh)
        return;

    AddRenderControlItem(pDrawList, &m_GCPanel, layer);
}

int SPanel::OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) {
    // Port of SControl.cpp:260-342 — coords arrive parent-relative.
    if (!m_bSelectEnable)
        return 0;

    int bInCaption = PointInRect((float)nX, (float)nY, m_nPosX, m_nPosY, m_nWidth, 24.0f);
    m_bOver = PointInRect((float)nX, (float)nY, m_nPosX, m_nPosY, m_nWidth, m_nHeight);

    if (m_bOver == 0 && m_pDescPanel)
        m_pDescPanel->SetVisible(0);

    switch (dwFlags) {
    case WM_MOUSEMOVE:
        if (m_bPicked && m_bPickable) {
            m_nPosX = (float)(nX - m_nPickPosX) + m_nPosX;
            m_nPosY = (float)(nY - m_nPickPosY) + m_nPosY;
            if (m_nPosX < 0.0f) m_nPosX = 0.0f;
            if (m_nPosY < 0.0f) m_nPosY = 0.0f;
            if (m_nPosX + m_nWidth > SControl::s_screenW)
                m_nPosX = SControl::s_screenW - m_nWidth;
            if (m_nPosY + m_nHeight > SControl::s_screenH)
                m_nPosY = SControl::s_screenH - m_nHeight;
            m_nPickPosX = nX;
            m_nPickPosY = nY;
        }
        if (m_bOver == 1 && (wParam & MK_LBUTTON_HELD))
            return 1;
        if (m_bOver == 1 && m_pDescPanel)
            m_pDescPanel->SetVisible(1);
        break;
    case WM_LBUTTONDOWN:
        if (m_bPickable && bInCaption) {
            m_bPicked = 1;
            m_nPickPosX = nX;
            m_nPickPosY = nY;
        }
        break;
    case WM_LBUTTONUP:
        m_bPicked = 0;
        break;
    }
    return 0;
}

// ---- SButton ----

SButton::SButton() {
    m_eCtrlType = CTRL_TYPE_BUTTON;
}

SButton::SButton(int textureSetIndex, float x, float y, float w, float h,
                 uint32_t color, int sound, const char* text)
    : SPanel(textureSetIndex, x, y, w, h, color, RENDER_IMAGE_STRETCH)
{
    m_eCtrlType = CTRL_TYPE_BUTTON;
    m_bSound = sound;
    if (text) {
        strncpy(m_GCPanel.strString, text, 255);
        m_GCPanel.strString[255] = '\0';
    }
}

void SButton::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                           int layer, int arg) {
    if (!m_bVisible) return;

    if (m_cBlink) {
        static int blinkCounter = 0;
        if (++blinkCounter % 20 == 0)
            m_GCPanel.nTextureIndex = (m_GCPanel.nTextureIndex == 0) ? 1 : 0;
    }

    if (!m_bEnable)
        m_GCPanel.nTextureIndex = 4;
    else if (m_bSelected)
        m_GCPanel.nTextureIndex = 3;
    else if (m_bPressed)
        m_GCPanel.nTextureIndex = 2;
    else if (m_bMouseOver)
        m_GCPanel.nTextureIndex = 1;
    else
        m_GCPanel.nTextureIndex = 0;

    SPanel::FrameMove2(pDrawList, parentPos, layer, arg);
}

int SButton::OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) {
    m_bOver = PointInRect((float)nX, (float)nY, m_nPosX, m_nPosY, m_nWidth, m_nHeight);
    m_bMouseOver = m_bOver;

    if (dwFlags == WM_LBUTTONDOWN && m_bOver && m_bEnable) {
        m_bPressed = 1;
        return 1;
    }
    if (dwFlags == WM_LBUTTONUP && m_bPressed) {
        m_bPressed = 0;
        if (m_bOver && m_pEventListener) {
            m_pEventListener->OnControlEvent(m_dwControlID, 0);
        }
        return 1;
    }

    return SPanel::OnMouseEvent(dwFlags, wParam, nX, nY);
}

// ---- SText ----

SText::SText() {
    m_eCtrlType = CTRL_TYPE_TEXT;
    memset(&m_GCText, 0, sizeof(m_GCText));
    memset(&m_GCBorder, 0, sizeof(m_GCBorder));
    m_GCText.eRenderType = RENDER_TEXT;
}

SText::SText(int textureSetIndex, const char* text, uint32_t fontColor,
             float x, float y, float w, float h,
             uint32_t borderColor, int border, int textType, int alignType)
    : SText()
{
    m_nPosX = x; m_nPosY = y;
    m_nWidth = w; m_nHeight = h;
    m_GCText.nTextureSetIndex = textureSetIndex;
    SetText(text);
    m_dwColor = fontColor;
    m_dwBorderColor = borderColor;
    m_nBorder = border;
    m_nTextType = textType;
    m_nAlignType = alignType;
}

void SText::SetText(const char* text) {
    if (text) {
        strncpy(m_szString, text, 255);
        m_szString[255] = '\0';
    } else {
        m_szString[0] = '\0';
    }
}

void SText::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                         int layer, int arg) {
    (void)arg;
    if (!m_bVisible || !m_szString[0]) return;

    m_GCText.eRenderType = (RENDERCTRLTYPE)m_nTextType;

    float textX = parentPos.x + m_nPosX;
    // Original heuristic: width ≈ 6*nLength or 7*nLength (RenderDevice.cpp:3302-3309)
    int len = (int)strlen(m_szString);
    float approxW = 6.0f * len * SControl::s_widthRatio;
    if (m_nAlignType == 1)       // center
        textX = parentPos.x + m_nPosX + (m_nWidth - approxW) * 0.5f;
    else if (m_nAlignType == 2)  // right
        textX = parentPos.x + m_nPosX + m_nWidth - approxW;

    float textY = parentPos.y + m_nPosY + (m_nHeight - 16 * SControl::s_heightRatio) * 0.5f + 2;

    if (textX + m_nWidth < 0 || textX > SControl::s_screenW ||
        textY + m_nHeight < 0 || textY > SControl::s_screenH)
        return;

    if (m_nBorder) {
        m_GCBorder.nPosX = textX;
        m_GCBorder.nPosY = textY;
        m_GCBorder.nWidth = m_nWidth;
        m_GCBorder.nHeight = m_nHeight;
        m_GCBorder.dwColor = m_dwBorderColor;
        m_GCBorder.eRenderType = RENDER_SHADOW;
        strncpy(m_GCBorder.strString, m_szString, 255);
        m_GCBorder.strString[255] = '\0';
        AddRenderControlItem(pDrawList, &m_GCBorder, layer);
    }

    m_GCText.nPosX = textX;
    m_GCText.nPosY = textY;
    m_GCText.nWidth = m_nWidth;
    m_GCText.nHeight = m_nHeight;
    m_GCText.dwColor = m_dwColor;
    strncpy(m_GCText.strString, m_szString, 255);
    m_GCText.strString[255] = '\0';
    AddRenderControlItem(pDrawList, &m_GCText, layer);
}

// ---- SEditableText ----

SEditableText::SEditableText() {
    m_eCtrlType = CTRL_TYPE_EDITABLETEXT;
}

SEditableText::SEditableText(int textureSetIndex, const char* text, int maxLen,
                              int passwd, uint32_t fontColor,
                              float x, float y, float w, float h)
    : SText()
{
    m_eCtrlType = CTRL_TYPE_EDITABLETEXT;
    m_nPosX = x; m_nPosY = y;
    m_nWidth = w; m_nHeight = h;
    m_GCText.nTextureSetIndex = textureSetIndex;
    m_nMaxLen = maxLen;
    m_bPassword = passwd;
    m_dwColor = fontColor;
    SetText(text);
    m_nCursorPos = (int)strlen(m_szString);
}

void SEditableText::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                                 int layer, int arg) {
    if (!m_bVisible) return;

    // Cursor blink over the stored string (SControl.cpp:1344-1373).
    static int frame = 0;
    frame++;
    bool showCursor = m_bFocused && (frame % 20 < 10);

    char saved[256];
    strncpy(saved, m_szString, 255);
    saved[255] = '\0';

    if (showCursor && (int)strlen(m_szString) < 255) {
        size_t len = strlen(m_szString);
        m_szString[len] = '|';
        m_szString[len + 1] = '\0';
    }
    SText::FrameMove2(pDrawList, parentPos, layer, arg);
    strncpy(m_szString, saved, 255);
    m_szString[255] = '\0';
}

int SEditableText::OnKeyDownEvent(int key) {
    (void)key;
    return 0;
}

int SEditableText::OnCharEvent(char c) {
    if (!m_bFocused) return 0;

    if (c == '\r') {
        if (m_pEventListener)
            m_pEventListener->OnControlEvent(m_dwControlID, 0);
        return 1;
    }
    if (c == '\t') {
        if (m_pEventListener)
            m_pEventListener->OnControlEvent(m_dwControlID, 1);
        return 1;
    }
    if (c == 27) {
        m_szString[0] = '\0';
        m_nCursorPos = 0;
        m_bFocused = 0;
        return 1;
    }
    if (c == 8) {
        if (m_nCursorPos > 0) {
            m_nCursorPos--;
            m_szString[m_nCursorPos] = '\0';
        }
        return 1;
    }
    int len = (int)strlen(m_szString);
    if (len < m_nMaxLen && len < 255) {
        m_szString[len] = c;
        m_szString[len + 1] = '\0';
        m_nCursorPos = len + 1;
    }
    return 1;
}

// ---- SScrollBar ----

SScrollBar::SScrollBar() {
    m_eCtrlType = CTRL_TYPE_SCROLLBAR;
}

SScrollBar::SScrollBar(float x, float y, float w, float h, int scrollRange)
    : SScrollBar()
{
    m_nPosX = x; m_nPosY = y;
    m_nWidth = w; m_nHeight = h;
    m_nScrollRange = scrollRange;
}

int SScrollBar::OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) {
    (void)dwFlags; (void)wParam; (void)nX; (void)nY;
    return 0;
}

// ---- SListBox ----

SListBox::SListBox() {
    m_eCtrlType = CTRL_TYPE_LISTBOX;
}

SListBox::SListBox(int textureSetIndex, int maxCount, int visibleCount,
                   float x, float y, float w, float h,
                   uint32_t color, int fillType, int select, int scroll)
    : SPanel(textureSetIndex, x, y, w, h, color, RENDER_IMAGE_STRETCH)
{
    (void)maxCount; (void)fillType; (void)select; (void)scroll;
    m_eCtrlType = CTRL_TYPE_LISTBOX;
    m_nVisibleCount = visibleCount;
}

void SListBox::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                            int layer, int arg) {
    if (!m_bVisible) return;
    SPanel::FrameMove2(pDrawList, parentPos, layer, arg);
}

int SListBox::OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) {
    return SPanel::OnMouseEvent(dwFlags, wParam, nX, nY);
}

int SListBox::OnControlEvent(uint32_t controlID, uint32_t event) {
    (void)controlID; (void)event;
    return 0;
}

int SListBox::AddItem(const char* str, uint32_t color) {
    if (m_nItemCount >= MAX_ITEMS) return -1;
    int idx = m_nItemCount++;
    strncpy(m_items[idx].str, str, 255);
    m_items[idx].str[255] = '\0';
    m_items[idx].color = color;
    return idx;
}

void SListBox::RemoveItem(int index) {
    if (index < 0 || index >= m_nItemCount) return;
    for (int i = index; i < m_nItemCount - 1; ++i)
        m_items[i] = m_items[i + 1];
    m_nItemCount--;
    if (m_nSelectedIndex >= m_nItemCount)
        m_nSelectedIndex = m_nItemCount - 1;
}

void SListBox::RemoveAll() {
    m_nItemCount = 0;
    m_nSelectedIndex = -1;
    m_nScrollPos = 0;
}

const char* SListBox::GetSelectedString() const {
    if (m_nSelectedIndex >= 0 && m_nSelectedIndex < m_nItemCount)
        return m_items[m_nSelectedIndex].str;
    return "";
}

// ---- SProgressBar ----

SProgressBar::SProgressBar() {
    m_eCtrlType = CTRL_TYPE_PROGRESSBAR;
}

SProgressBar::SProgressBar(int textureSetIndex, int current, int max,
                           float x, float y, float w, float h,
                           uint32_t progressColor, uint32_t color, int style)
    : SPanel(textureSetIndex, x, y, w, h, color, RENDER_IMAGE_STRETCH)
{
    m_eCtrlType = CTRL_TYPE_PROGRESSBAR;
    m_nCurrent = current;
    m_nMax = max;
    m_dwProgressColor = progressColor;
    m_nStyle = style;
}

void SProgressBar::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                                int layer, int arg) {
    if (!m_bVisible) return;
    SPanel::FrameMove2(pDrawList, parentPos, layer, arg);

    float ratio = m_nMax > 0 ? (float)m_nCurrent / (float)m_nMax : 0;
    if (ratio > 1.0f) ratio = 1.0f;
    if (m_nStyle == 1)
        m_GCPanel.nWidth = m_nWidth * ratio;
    else
        m_GCPanel.nHeight = m_nHeight * ratio;
}

// ---- SCheckBox ----

SCheckBox::SCheckBox() {
    m_eCtrlType = CTRL_TYPE_CHECKBOX;
}

SCheckBox::SCheckBox(int textureSetIndex, float x, float y, float w, float h, uint32_t color)
    : SPanel(textureSetIndex, x, y, w, h, color, RENDER_IMAGE_STRETCH)
{
    m_eCtrlType = CTRL_TYPE_CHECKBOX;
}

int SCheckBox::OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) {
    if (dwFlags == WM_LBUTTONUP && m_bOver) {
        m_bChecked = !m_bChecked;
        m_GCPanel.nTextureIndex = m_bChecked ? 1 : 0;
        if (m_pEventListener)
            m_pEventListener->OnControlEvent(m_dwControlID, m_bChecked ? 1 : 0);
        return 1;
    }
    return SPanel::OnMouseEvent(dwFlags, wParam, nX, nY);
}

// ---- SMessageBox ----

SMessageBox::SMessageBox() {
    m_eCtrlType = CTRL_TYPE_MESSAGEBOX;
}

SMessageBox::SMessageBox(int textureSetIndex, const char* text,
                         float x, float y, float w, float h, uint32_t color)
    : SPanel(textureSetIndex, x, y, w, h, color, RENDER_IMAGE_STRETCH)
{
    (void)text;
    m_eCtrlType = CTRL_TYPE_MESSAGEBOX;
    m_bModal = 1;
}

int SMessageBox::OnCharEvent(char c) {
    if (c == 'Y' || c == 'y' || c == '\r') {
        if (m_pEventListener)
            m_pEventListener->OnControlEvent(m_dwControlID, 0);
        return 1;
    }
    if (c == 'N' || c == 'n') {
        if (m_pEventListener)
            m_pEventListener->OnControlEvent(m_dwControlID, 1);
        return 1;
    }
    return 0;
}

int SMessageBox::OnControlEvent(uint32_t controlID, uint32_t event) {
    (void)controlID;
    if (m_pEventListener)
        m_pEventListener->OnControlEvent(m_dwControlID, event);
    return 1;
}

// ---- SMessagePanel ----

SMessagePanel::SMessagePanel() {
    m_eCtrlType = CTRL_TYPE_MESSAGEPANEL;
}

SMessagePanel::SMessagePanel(int textureSetIndex, float x, float y,
                              float w, float h, uint32_t color)
    : SPanel(textureSetIndex, x, y, w, h, color, RENDER_IMAGE_STRETCH)
{
    m_eCtrlType = CTRL_TYPE_MESSAGEPANEL;
}

void SMessagePanel::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                                 int layer, int arg) {
    SPanel::FrameMove2(pDrawList, parentPos, layer, arg);
}

// ---- SCursor ----

SCursor::SCursor() {
    m_eCtrlType = CTRL_TYPE_CURSOR;
}

void SCursor::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                           int layer, int arg) {
    (void)parentPos;
    if (m_nCursorType == 2) {
        m_GCPanel.nPosX = -100;  // invisible
        return;
    }
    if (m_pAttachedItem) {
        m_pAttachedItem->m_GCPanel.nPosX = m_nPosX;
        m_pAttachedItem->m_GCPanel.nPosY = m_nPosY;
        AddRenderControlItem(pDrawList, &m_pAttachedItem->m_GCPanel, layer);
    }
    SPanel::FrameMove2(pDrawList, IVector2{0, 0}, layer, arg);
}

// ---- SGridControl ----

SGridControl::SGridControl() {
    m_eCtrlType = CTRL_TYPE_GRID;
}

SGridControl::SGridControl(int textureSetIndex, int rows, int cols,
                            float x, float y, float w, float h, int type)
    : SGridControl()
{
    m_nPosX = x; m_nPosY = y;
    m_nWidth = w; m_nHeight = h;
    m_nRows = rows;
    m_nCols = cols;
    m_nType = type;

    for (int i = 0; i < 64; ++i) {
        memset(&m_Cells[i], 0, sizeof(GeomControl));
        m_Cells[i].eRenderType = RENDER_IMAGE_STRETCH;
        m_Cells[i].nTextureSetIndex = textureSetIndex;
    }
}

void SGridControl::FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                                int layer, int arg) {
    (void)arg;
    if (!m_bVisible) return;
    if (m_nCols <= 0 || m_nRows <= 0) return;
    float cellW = m_nWidth / m_nCols;
    float cellH = m_nHeight / m_nRows;
    for (int r = 0; r < m_nRows && r < 8; ++r) {
        for (int c = 0; c < m_nCols && c < 8; ++c) {
            int idx = r * m_nCols + c;
            if (idx >= 64) break;
            m_Cells[idx].nPosX = parentPos.x + m_nPosX + c * cellW;
            m_Cells[idx].nPosY = parentPos.y + m_nPosY + r * cellH;
            m_Cells[idx].nWidth = cellW;
            m_Cells[idx].nHeight = cellH;
            AddRenderControlItem(pDrawList, &m_Cells[idx], layer);
        }
    }
}

}
