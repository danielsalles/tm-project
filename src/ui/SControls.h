#pragma once

#include "ui/SControl.h"
#include "ui/UITypes.h"

namespace tmx {

class GLFont;
class SText;
class SScrollBar;

// SPanel — textured rectangle (base for most controls).
class SPanel : public SControl {
public:
    SPanel();
    SPanel(int textureSetIndex, float x, float y, float w, float h,
           uint32_t color, int renderType);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;
    int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) override;

    GeomControl m_GCPanel;
    SPanel* m_pDescPanel = nullptr;  // tooltip shown on hover
    int m_bPickable = 0;
    int m_bPicked = 0;
    int m_nPickPosX = 0, m_nPickPosY = 0;
};

// SButton — 4-state button (common/over/press/selected).
class SButton : public SPanel {
public:
    SButton();
    SButton(int textureSetIndex, float x, float y, float w, float h,
            uint32_t color, int sound, const char* text);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;
    int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) override;

    int m_bMouseOver = 0;
    int m_bPressed = 0;
    int m_bSelected = 0;
    char m_cBlink = 0;
    int m_GrayType = 0;
    int m_bSound = 0;
    SText* m_pAltText = nullptr;
};

// SText — static text.
class SText : public SControl {
public:
    SText();
    SText(int textureSetIndex, const char* text, uint32_t fontColor,
          float x, float y, float w, float h,
          uint32_t borderColor, int border, int textType, int alignType);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;

    void SetText(const char* text);
    const char* GetText() const { return m_szString; }

    GeomControl m_GCText;
    GeomControl m_GCBorder;

    uint32_t m_dwColor = 0xFFFFFFFF;
    uint32_t m_dwBorderColor = 0xFF000000;
    int m_nBorder = 0;
    int m_nTextType = 0;   // RENDER_TEXT / RENDER_SHADOW
    int m_nAlignType = 0;  // 0=left, 1=center, 2=right, 3=no-margin
    char m_szString[256] = {};
};

// SEditableText — text input with cursor blink.
class SEditableText : public SText {
public:
    SEditableText();
    SEditableText(int textureSetIndex, const char* text, int maxLen,
                  int passwd, uint32_t fontColor,
                  float x, float y, float w, float h);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;
    int OnKeyDownEvent(int key) override;
    int OnCharEvent(char c) override;

    // IME composition in progress (SDL_EVENT_TEXT_EDITING, phase 7): rendered
    // after the committed text with an underline, cleared on commit.
    void SetComposition(const char* text);
    const char* GetComposition() const { return m_szComposition; }

    int m_nMaxLen = 128;
    int m_bPassword = 0;
    int m_nCursorPos = 0;

private:
    char m_szComposition[256] = {};
    GeomControl m_GCComposition;   // composition text (highlight color)
    GeomControl m_GCCompUnderline; // 1px solid rect under it
};

// SScrollBar — vertical scroll bar.
class SScrollBar : public SControl {
public:
    SScrollBar();
    SScrollBar(float x, float y, float w, float h, int scrollRange);

    int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) override;

    int m_nScrollPos = 0;
    int m_nScrollRange = 100;
};

// SListBox — scrollable list of items.
class SListBox : public SPanel, public IEventListener {
public:
    SListBox();
    SListBox(int textureSetIndex, int maxCount, int visibleCount,
             float x, float y, float w, float h,
             uint32_t color, int fillType, int select, int scroll);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;
    int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) override;
    int OnControlEvent(uint32_t controlID, uint32_t event) override;

    int AddItem(const char* str, uint32_t color = 0xFFFFFFFF);
    void RemoveItem(int index);
    void RemoveAll();
    int GetSelectedIndex() const { return m_nSelectedIndex; }
    const char* GetSelectedString() const;

    static constexpr int MAX_ITEMS = 256;
    struct ListBoxItem {
        char str[256] = {};
        uint32_t color = 0xFFFFFFFF;
        int selected = 0;
    };
    ListBoxItem m_items[MAX_ITEMS];
    int m_nItemCount = 0;
    int m_nVisibleCount = 5;
    int m_nSelectedIndex = -1;
    int m_nScrollPos = 0;
    SScrollBar* m_pScrollBar = nullptr;
};

// SProgressBar — horizontal/vertical progress bar.
class SProgressBar : public SPanel {
public:
    SProgressBar();
    SProgressBar(int textureSetIndex, int current, int max,
                 float x, float y, float w, float h,
                 uint32_t progressColor, uint32_t color, int style);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;

    int m_nCurrent = 0;
    int m_nMax = 100;
    uint32_t m_dwProgressColor = 0xFF00FF00;
    int m_nStyle = 0;  // 0=vertical, 1=horizontal
};

// SCheckBox — toggle checkbox.
class SCheckBox : public SPanel {
public:
    SCheckBox();
    SCheckBox(int textureSetIndex, float x, float y, float w, float h, uint32_t color);

    int OnMouseEvent(unsigned int dwFlags, unsigned int wParam, int nX, int nY) override;

    int m_bChecked = 0;
};

// SMessageBox — modal dialog with OK/Cancel.
class SMessageBox : public SPanel, public IEventListener {
public:
    SMessageBox();
    SMessageBox(int textureSetIndex, const char* text,
                float x, float y, float w, float h, uint32_t color);

    int OnCharEvent(char c) override;
    int OnControlEvent(uint32_t controlID, uint32_t event) override;

    SButton* m_pOKButton = nullptr;
    SButton* m_pCancelButton = nullptr;
};

// SMessagePanel — auto-hiding notification bar.
class SMessagePanel : public SPanel {
public:
    SMessagePanel();
    SMessagePanel(int textureSetIndex, float x, float y,
                  float w, float h, uint32_t color);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;

    uint32_t m_dwShowTime = 0;
    uint32_t m_dwLifeTime = 3000;
};

// SCursor — mouse cursor (software rendered, layer 29).
class SCursor : public SPanel {
public:
    SCursor();

    void SetPosition(float x, float y) { m_nPosX = x; m_nPosY = y; }
    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;

    int m_nCursorType = 0;
    SPanel* m_pAttachedItem = nullptr;
};

// SGridControl — grid layout (inventory, skill bar).
class SGridControl : public SControl {
public:
    SGridControl();
    SGridControl(int textureSetIndex, int rows, int cols,
                 float x, float y, float w, float h, int type);

    void FrameMove2(stGeomList* pDrawList, IVector2 parentPos,
                    int layer, int arg) override;

    int m_nRows = 1;
    int m_nCols = 1;
    int m_nType = 0;
    GeomControl m_Cells[64];  // max 8x8 grid
};

}
