#pragma once

#include "pane/GuiMat.h"
#include "pane/PaneFile.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tmx {

class UIRenderer;
class GLFont;

// Runtime view of a loaded .pane: layout (AlignPanel*), rendering and input.
// Texture resolution is injected (keeps the view headless-testable).
//
// Rendering semantics (from the decrypted panes + default.guimat):
//   Panel     — optional TextureAlias background stretched to Width×Height;
//               Titlebar true draws NewUI.WindowBGTitle strip + Caption.
//   Image     — TextureAlias (or ExternalImageFile) stretched to Area.
//   Static    — Caption/Text drawn with FontAlign ("center vcenter" etc.).
//   Button    — UI.ButtonN/O/D by state; caption centered.
//   Editbox   — NewUI.EditBG when DrawBox; text left vcenter; Password masks;
//   ImeEditbox— same, plus IME composition.
//   ListView  — TextureAlias[1] border only (items need the server list, 8c).
//
// Captions "uu_NN" resolve through UILoader::UIString(NN) (the current client
// maps uu_NN → Lang UIString.txt entry NN).
class PaneView {
public:
    // textureResolver: path (from guimat, e.g. "NUI/ServerList.wyt") → GL
    // texture + pixel size. Return false if unloadable.
    using TextureResolver =
        std::function<bool(const std::string& path, uint32_t& tex, int& w, int& h)>;

    bool Load(PaneFile file, const GuiMat* mat);
    void SetTextureResolver(TextureResolver r) { m_resolver = std::move(r); }

    // AlignPanelX/Y + Offset against the real screen size (idempotent).
    void Layout(int screenW, int screenH);

    void Render(UIRenderer* ui, GLFont* font);

    // Input. Coordinates in absolute screen pixels.
    bool OnMouseMove(int x, int y);
    bool OnMouseDown(int x, int y);           // returns true if consumed
    bool OnMouseUp(int x, int y);             // may set LastClick()
    void OnChar(char c);
    void Backspace();
    // TAB cycles editable fields (client behavior on the login panes).
    void FocusNext();
    bool HasFocus() const;

    const std::string& LastClick() const { return m_lastClick; }
    void ClearClick() { m_lastClick.clear(); }

    const std::string& GetText(const std::string& name) const;
    void SetText(const std::string& name, const std::string& text);
    void SetVisibleByName(const std::string& name, bool visible);
    void SetVisibleByTag(const std::string& tag, bool visible);
    // Adjust an element's parent-relative rect by its uu caption id
    // (header Statics have no name — e.g. "uu_03").
    void AdjustRectByCaptionId(const std::string& captionId, int x, int y, int w, int h);
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool v) { m_visible = v; }

    // ListView population (phase 8c): rows are ItemSize tall, text via the
    // item template's FontAlign. Selection is a single index.
    void SetListItems(const std::string& name, const std::vector<std::string>& items);
    // Parallel gauge values (channel fullness, BarRange 0..500); -1 hides.
    void SetListItemValues(const std::string& name, const std::vector<int>& values);
    // Parallel per-item flags: bit0 = crown icon (castle channel), bit1 =
    // recommended-day channel (gauge green instead of red).
    void SetListItemFlags(const std::string& name, const std::vector<uint32_t>& flags);
    int GetListSelection(const std::string& name) const;

    int X() const { return m_x; }
    int Y() const { return m_y; }
    int Width() const { return m_w; }
    int Height() const { return m_h; }

private:
    struct Node {
        std::string type;
        std::string name;
        std::string tag;
        int rx = 0, ry = 0;           // Area position (parent-relative)
        int w = 0, h = 0;
        int x = 0, y = 0;             // absolute, recomputed by Layout (idempotent)
        bool visible = true;
        bool hovered = false;
        bool pressed = false;
        bool focused = false;
        bool password = false;
        bool drawBox = false;
        std::string caption;    // resolved text (uu_NN or Text)
        std::string captionId;  // raw id when sourced from uu_NN (e.g. "uu_03")
        std::string fontAlign;  // e.g. "center vcenter"
        std::string text;       // edit contents
        // ListView state
        int itemW = 0, itemH = 18;
        std::vector<std::string> items;
        std::vector<int> itemValues;       // parallel: gauge value 0..500, -1 = no gauge
        std::vector<uint32_t> itemFlags;   // parallel: bit0=crown, bit1=recommended
        int selected = -1;
        int hoverRow = -1;
        const PaneElement* src = nullptr;
        std::vector<Node> children;
    };

    void BuildNode(const PaneElement& el, Node& out);
    void LayoutNode(Node& n, int baseX, int baseY);
    void RenderNode(Node& n, UIRenderer* ui, GLFont* font);
    Node* FindNode(const std::string& name);
    const Node* FindNode(const std::string& name) const;
    Node* HitTest(int x, int y);
    void RenderEditText(Node& n, UIRenderer* ui, GLFont* font);
    const char* PanelProp(const char* key) const;

    void DrawAlias(const std::string& aliasName, int dx, int dy, int dw, int dh,
                   UIRenderer* ui, uint32_t color = 0xFFFFFFFF);
    // text with FontAlign inside rect; strColor ARGB
    void DrawText(const std::string& s, const std::string& align,
                  int x, int y, int w, int h, uint32_t color,
                  UIRenderer* ui, GLFont* font);
    // same, with a 1px black drop shadow (SListBoxItem SetType(1), the look
    // the client uses for list items)
    void DrawTextShadow(const std::string& s, const std::string& align,
                        int x, int y, int w, int h, uint32_t color,
                        UIRenderer* ui, GLFont* font);

    std::string ResolveCaption(const std::string& raw) const;

    Node m_root;
    PaneFile m_file;  // owned: Node::src points into m_file.elements
    std::vector<std::pair<std::string, std::string>> m_panelProps;
    const GuiMat* m_mat = nullptr;
    TextureResolver m_resolver;
    bool m_visible = true;
    int m_x = 0, m_y = 0, m_w = 0, m_h = 0;
    int m_screenW = 0, m_screenH = 0;
    std::string m_lastClick;
};

} // namespace tmx
