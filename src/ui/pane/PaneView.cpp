#include "pane/PaneView.h"

#include "gl/GLFont.h"
#include "gl/UIRenderer.h"
#include "ui/UILoader.h"

#include <cstdio>
#include <cstring>

namespace tmx {

bool PaneView::Load(PaneFile file, const GuiMat* mat) {
    m_mat = mat;
    m_file = std::move(file);  // own it: Node::src points into m_file.elements
    m_w = m_file.Get("Width") ? atoi(m_file.Get("Width")) : 0;
    m_h = m_file.Get("Height") ? atoi(m_file.Get("Height")) : 0;

    m_root = Node{};
    m_root.type = "Panel";
    for (const auto& el : m_file.elements) {
        Node child;
        BuildNode(el, child);
        m_root.children.push_back(std::move(child));
    }
    // panel-level background/caption live on the root node
    m_root.src = nullptr;
    m_root.caption = m_file.Get("Caption") ? m_file.Get("Caption") : "";
    m_root.name = "__panel";
    m_root.tag = "";
    // keep panel props accessible through a synthetic node
    m_panelProps = m_file.props;
    return true;
}

void PaneView::BuildNode(const PaneElement& el, Node& out) {
    out.src = &el;
    out.type = el.type;
    out.name = el.name;
    if (const char* t = el.Get("Tagname"))
        out.tag = t;
    if (const char* v = el.Get("Visible"))
        out.visible = (v != std::string("false"));
    out.password = el.Get("Password") && std::string(el.Get("Password")) == "true";
    out.drawBox = el.Get("DrawBox") && std::string(el.Get("DrawBox")) == "true";
    if (const char* c = el.Get("Caption")) {
        out.captionId = c;
        out.caption = ResolveCaption(c);
    }
    if (const char* t = el.Get("Text"))
        out.caption = t;
    if (const char* fa = el.Get("FontAlign"))
        out.fontAlign = fa;
    else if (const char* fa0 = el.Get("FontAlign[0]"))
        out.fontAlign = fa0;
    // Area is "x y width height" (NOT x1 y1 x2 y2 — validated: GameLogin's
    // two ServerBG images tile 1+152=153, 153+175=328 across the 331 panel).
    int x = 0, y = 0, w = 0, h = 0;
    if (ParseArea(el.Get("Area"), x, y, w, h)) {
        out.rx = x; out.ry = y;
        out.w = w; out.h = h;
    }
    if (out.type == "ListView") {
        int iw = 0, ih = 0;
        if (sscanf(el.Get("ItemSize") ? el.Get("ItemSize") : "", "%d %d", &iw, &ih) == 2) {
            out.itemW = iw;
            out.itemH = ih;
        }
    }
    for (const auto& ch : el.children) {
        Node cn;
        BuildNode(ch, cn);
        out.children.push_back(std::move(cn));
    }
}

void PaneView::Layout(int screenW, int screenH) {
    m_screenW = screenW;
    m_screenH = screenH;

    const char* ax = PanelProp("AlignPanelX");
    const char* ay = PanelProp("AlignPanelY");
    float ox = PanelProp("OffsetX") ? (float)atof(PanelProp("OffsetX")) : 0.0f;
    float oy = PanelProp("OffsetY") ? (float)atof(PanelProp("OffsetY")) : 0.0f;

    if (ax && !strcmp(ax, "Center"))
        m_x = (screenW - m_w) / 2 + (int)ox;
    else if (ax && !strcmp(ax, "SIDE"))
        m_x = (screenW - m_w) / 2 + (int)ox;  // SIDE: centered horizontally
    else
        m_x = (int)ox;

    if (ay && !strcmp(ay, "Center"))
        m_y = (screenH - m_h) / 2 + (int)oy;
    else if (ay && !strcmp(ay, "SIDE"))
        m_y = screenH - m_h + (int)oy;  // SIDE on Y docks to the bottom (copyright bar)
    else
        m_y = (int)oy;

    for (auto& ch : m_root.children)
        LayoutNode(ch, m_x, m_y);
}

void PaneView::LayoutNode(Node& n, int baseX, int baseY) {
    n.x = n.rx + baseX;   // recompute from parent-relative coords (idempotent)
    n.y = n.ry + baseY;
    for (auto& ch : n.children)
        LayoutNode(ch, n.x, n.y);
}

const char* PaneView::PanelProp(const char* key) const {
    for (auto& kv : m_panelProps) {
        if (kv.first == key)
            return kv.second.c_str();
    }
    return nullptr;
}

std::string PaneView::ResolveCaption(const std::string& raw) const {
    // uu_NN → UIString.txt entry NN (client convention). Underscores → spaces.
    if (raw.rfind("uu_", 0) == 0) {
        int idx = atoi(raw.c_str() + 3);
        std::string s = UILoader::UIString(idx);
        for (auto& c : s) {
            if (c == '_') c = ' ';
        }
        // Trim leading padding spaces: the original center-aligns WITH them
        // (that's why "Servidor"/"Canal" look skewed even in the official
        // exe); we want straight headers at any resolution.
        const size_t b = s.find_first_not_of(' ');
        if (b != std::string::npos)
            s.erase(0, b);
        return s;
    }
    return raw;
}

void PaneView::DrawAlias(const std::string& aliasName, int dx, int dy, int dw, int dh,
                         UIRenderer* ui, uint32_t color) {
    if (!m_mat || !m_resolver || aliasName.empty())
        return;
    const GuiMat::Alias* al = m_mat->FindAlias(aliasName);
    if (!al)
        return;
    const GuiMat::Texture* tex = m_mat->FindTexture(al->texIndex);
    if (!tex)
        return;
    uint32_t gl = 0;
    int tw = 0, th = 0;
    if (!m_resolver(tex->path, gl, tw, th) || !gl)
        return;
    ui->RenderRectC((float)al->x1, (float)al->y1, (float)al->Width(), (float)al->Height(),
                    (float)dx, (float)dy, gl, tw, th, color,
                    al->Width()  > 0 ? (float)dw / al->Width()  : 1.0f,
                    al->Height() > 0 ? (float)dh / al->Height() : 1.0f);
}

void PaneView::DrawText(const std::string& s, const std::string& align,
                        int x, int y, int w, int h, uint32_t color,
                        UIRenderer* ui, GLFont* font) {
    if (s.empty() || !font)
        return;
    font->SetNoWrap(true);  // pane texts render single-line (not TMFont2 42-char)
    font->SetText(s.c_str(), color);
    const float tw = (float)font->GetLastWidth();
    const float th = (float)font->GetLastHeight();

    float tx = (float)x;
    if (align.find("center") != std::string::npos)
        tx = x + (w - tw) * 0.5f;
    else if (align.find("right") != std::string::npos)
        tx = x + w - tw;

    float ty = (float)y;
    if (align.find("vcenter") != std::string::npos)
        ty = y + (h - th) * 0.5f;
    else if (align.find("bottom") != std::string::npos)
        ty = y + h - th;

    font->Render(ui->Batch(), tx, ty, 0, 0);
}

void PaneView::DrawTextShadow(const std::string& s, const std::string& align,
                              int x, int y, int w, int h, uint32_t color,
                              UIRenderer* ui, GLFont* font) {
    if (s.empty() || !font)
        return;
    font->SetNoWrap(true);
    font->SetText(s.c_str(), color);
    const float tw = (float)font->GetLastWidth();
    const float th = (float)font->GetLastHeight();
    float tx = (float)x;
    if (align.find("center") != std::string::npos)
        tx = x + (w - tw) * 0.5f;
    else if (align.find("right") != std::string::npos)
        tx = x + w - tw;
    float ty = (float)y;
    if (align.find("vcenter") != std::string::npos)
        ty = y + (h - th) * 0.5f;
    else if (align.find("bottom") != std::string::npos)
        ty = y + h - th;
    font->Render(ui->Batch(), tx, ty, 1 /*shadow*/, 0);
}

void PaneView::Render(UIRenderer* ui, GLFont* font) {
    if (!m_visible || !ui)
        return;

    // Panel background. NB: no titlebar art — the client's titlebar material
    // (panel_0 block of Default.guimat) has color_default alpha=0, so the
    // renderer skips it entirely (WYD 769.2.c:111010 sub_477100). The old
    // NewUI.WindowBGTitle strip we drew here was never referenced by the
    // client — it was the stray blue band behind the login panel.
    const char* panelAlias = PanelProp("TextureAlias");
    if (panelAlias && *panelAlias)
        DrawAlias(panelAlias, m_x, m_y, m_w, m_h, ui);
    // Caption text only (rect {x+3, y+3, w-6, titlebar_height}, WYD
    // 769.2.c:112315-112330) — empty Caption draws nothing.
    if (PanelProp("Titlebar") && !strcmp(PanelProp("Titlebar"), "true")) {
        const char* cap = PanelProp("Caption");
        if (cap && *cap) {
            int tbh = PanelProp("TitlebarHeight") ? atoi(PanelProp("TitlebarHeight")) : 30;
            DrawText(ResolveCaption(cap), "center vcenter",
                     m_x + 3, m_y + 3, m_w - 6, tbh, 0xFFFFFFFF, ui, font);
        }
    }

    for (auto& ch : m_root.children)
        RenderNode(ch, ui, font);
}

void PaneView::RenderNode(Node& n, UIRenderer* ui, GLFont* font) {
    if (!n.visible)
        return;

    if (n.type == "Image") {
        const char* ext = n.src ? n.src->Get("ExternalImageFile") : nullptr;
        const char* alias = n.src ? n.src->Get("TextureAlias") : nullptr;
        if (ext && *ext && m_resolver) {
            uint32_t gl = 0;
            int tw = 0, th = 0;
            if (m_resolver(ext, gl, tw, th) && gl)
                ui->RenderRectC(0, 0, (float)tw, (float)th, (float)n.x, (float)n.y,
                                gl, tw, th, 0xFFFFFFFF,
                                tw > 0 ? (float)n.w / tw : 1.0f,
                                th > 0 ? (float)n.h / th : 1.0f);
        } else if (alias && *alias) {
            DrawAlias(alias, n.x, n.y, n.w, n.h, ui);
        }
    } else if (n.type == "Static") {
        DrawText(n.caption, n.fontAlign.empty() ? "left vcenter" : n.fontAlign,
                 n.x, n.y, n.w, n.h, 0xFFFFFFFF, ui, font);
    } else if (n.type == "Button") {
        // The pane Button draws ONE texture and only tints by state — hover
        // is visually a no-op in the default skin (color_mouseover undefined;
        // WYD 769.2.c:68431-68460). No art swap; pressed darkens slightly.
        DrawAlias("UI.ButtonN", n.x, n.y, n.w, n.h, ui,
                  n.pressed ? 0xFFBBBBBB : 0xFFFFFFFF);
        DrawText(n.caption, "center vcenter", n.x, n.y, n.w, n.h, 0xFFFFFFFF, ui, font);
    } else if (n.type == "Editbox" || n.type == "ImeEditbox") {
        if (n.drawBox)
            DrawAlias("NewUI.EditBG", n.x, n.y, n.w, n.h, ui);
        RenderEditText(n, ui, font);
    } else if (n.type == "ListView") {
        // Selection: the client draws the listbox_1 selection material
        // (NewUI.Empty, a hollow border texture) over the row — tinted opaque
        // when selected (color_pressed), 50% on hover (color_mouseover).
        // Text color NEVER changes (WYD 769.2.c:91451-91479).
        const int rowH = n.itemH > 0 ? n.itemH : 18;
        for (size_t i = 0; i < n.items.size(); ++i) {
            const int ry = n.y + (int)i * rowH;
            if (ry + rowH > n.y + n.h)
                break;
            const bool sel = (int)i == n.selected;
            const bool hov = (int)i == n.hoverRow && !sel;
            // Selection border: the client tints a fully-transparent patch
            // (NewUI.Empty is alpha-0) with the state color, producing a
            // border frame around the row — pressed = opaque, hover = 50%.
            if (sel || hov) {
                const uint32_t bc = sel ? 0xFFFFFFFF : 0x80FFFFFF;
                const float x0 = (float)n.x, y0 = (float)ry;
                const float x1 = x0 + n.w, y1 = y0 + rowH;
                ui->RenderRectNoTex(x0, y0, n.w, 1.0f, bc, false);
                ui->RenderRectNoTex(x0, y1 - 1.0f, n.w, 1.0f, bc, false);
                ui->RenderRectNoTex(x0, y0, 1.0f, rowH, bc, false);
                ui->RenderRectNoTex(x1 - 1.0f, y0, 1.0f, rowH, bc, false);
            }

            const int value = i < n.itemValues.size() ? n.itemValues[i] : -1;
            if (value >= 0) {
                // Channel rows: the item string centers in the 81px name zone
                // (GameLoginItem2.pane Static(Name) = center vcenter). The
                // padded "...FULL" label (col 14) lands at/above the gauge —
                // symmetric overflow, never breaks layout (WYD 769.2.c:212902).
                DrawTextShadow(n.items[i], "center vcenter", n.x, ry, 81, rowH,
                               0xFFFFFFFF, ui, font);
                const uint32_t flags = i < n.itemFlags.size() ? n.itemFlags[i] : 0;
                const int bx = n.x + 81;
                const int by = ry + 6;
                if (flags & 4) {
                    // FULL (our style): no gauge at all — the word rendered
                    // with the same font/size as the channel names, centered
                    // where the bar would be.
                    DrawTextShadow("FULL", "center vcenter", bx, ry, 25, rowH,
                                   0xFFFFFFFF, ui, font);
                } else {
                    // Gauge 0..500 (SetRange(0,500), WYD 769.2.c:213075);
                    // counts <300 arrive already doubled (fillChannels). Red
                    // for normal channels, green for the day's recommended
                    // one (WYD 769.2.c:213076-213079).
                    ui->RenderRectNoTex((float)bx, (float)by, 25.0f, 5.0f,
                                        0xFF222222, false);
                    const int fill = value > 500 ? 500 : value;
                    const float fw = 25.0f * fill / 500.0f;
                    const uint32_t fc = (flags & 2) ? 0xFF00FF99 : 0xFFFF0000;
                    if (fw > 0.0f)
                        ui->RenderRectNoTex((float)bx, (float)by, fw, 5.0f, fc, false);
                }

                // Crown icon (castle channel, WYD 769.2.c:213146) at x+125
                // per GameLoginItem2.pane. Devil/Age are never shown by the
                // client (SetVisible(0), WYD 769.2.c:213112/213183).
                if (flags & 1)
                    DrawAlias("UI_Login.Crown", n.x + 125, ry, 18, 18, ui);
            } else {
                // Group rows (GameLoginItem.pane): name centered, full width.
                DrawTextShadow(n.items[i], "center vcenter", n.x, ry, n.w, rowH,
                               0xFFFFFFFF, ui, font);
            }
        }
    }

    for (auto& ch : n.children)
        RenderNode(ch, ui, font);
}

void PaneView::RenderEditText(Node& n, UIRenderer* ui, GLFont* font) {
    std::string shown = n.password ? std::string(n.text.size(), '*') : n.text;
    // cursor blink (same cadence as SEditableText)
    static int blinkFrame = 0;
    if (n.focused && (++blinkFrame % 40 < 20))
        shown += '|';
    DrawText(shown, "left vcenter", n.x + 3, n.y, n.w - 3, n.h, 0xFFFFFFFF, ui, font);
}

PaneView::Node* PaneView::HitTest(int x, int y) {
    Node* best = nullptr;
    // topmost = last matching child (render order)
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n->visible)
            return;
        for (auto& ch : n->children)
            walk(&ch);
        if (x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h) {
            if (n->type == "Button" || n->type == "Editbox" || n->type == "ImeEditbox" ||
                (n->type == "ListView" && !n->items.empty()))
                best = n;
        }
    };
    for (auto& ch : m_root.children)
        walk(&ch);
    return best;
}

bool PaneView::OnMouseMove(int x, int y) {
    bool any = false;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n->visible)
            return;
        bool over = x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h;
        if (n->type == "Button") {
            n->hovered = over;
            any = any || over;
        }
        if (n->type == "ListView") {
            const int rowH = n->itemH > 0 ? n->itemH : 18;
            n->hoverRow = over ? (y - n->y) / rowH : -1;
            if (n->hoverRow >= (int)n->items.size())
                n->hoverRow = -1;
        }
        for (auto& ch : n->children)
            walk(&ch);
    };
    for (auto& ch : m_root.children)
        walk(&ch);
    return any;
}

bool PaneView::OnMouseDown(int x, int y) {
    Node* hit = HitTest(x, y);
    if (!hit)
        return false;
    if (hit->type == "Button") {
        hit->pressed = true;
        return true;
    }
    if (hit->type == "Editbox" || hit->type == "ImeEditbox") {
        std::function<void(Node*)> clear = [&](Node* n) {
            n->focused = false;
            for (auto& c : n->children) clear(&c);
        };
        for (auto& ch : m_root.children) clear(&ch);
        hit->focused = true;
        return true;
    }
    if (hit->type == "ListView") {
        const int rowH = hit->itemH > 0 ? hit->itemH : 18;
        const int row = (y - hit->y) / rowH;
        if (row >= 0 && row < (int)hit->items.size())
            hit->selected = row;
        return true;
    }
    return false;
}

bool PaneView::OnMouseUp(int x, int y) {
    bool consumed = false;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (n->type == "Button" && n->pressed) {
            n->pressed = false;
            if (x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h) {
                m_lastClick = n->name;
                consumed = true;
            }
        }
        for (auto& ch : n->children)
            walk(&ch);
    };
    for (auto& ch : m_root.children)
        walk(&ch);
    return consumed;
}

void PaneView::OnChar(char c) {
    std::function<void(Node*)> walk = [&](Node* n) {
        if (n->focused && (n->type == "Editbox" || n->type == "ImeEditbox")) {
            if (c == 8) {
                if (!n->text.empty()) n->text.pop_back();
            } else if (c >= 32 && c != 127 && n->text.size() < 60) {
                n->text += c;
            }
        }
        for (auto& ch : n->children) walk(&ch);
    };
    for (auto& ch : m_root.children) walk(&ch);
}

void PaneView::Backspace() {
    OnChar(8);
}

void PaneView::FocusNext() {
    std::vector<Node*> edits;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (n->type == "Editbox" || n->type == "ImeEditbox")
            edits.push_back(n);
        for (auto& ch : n->children) walk(&ch);
    };
    for (auto& ch : m_root.children) walk(&ch);
    if (edits.empty())
        return;
    int cur = -1;
    for (size_t i = 0; i < edits.size(); ++i) {
        if (edits[i]->focused) { cur = (int)i; edits[i]->focused = false; break; }
    }
    edits[(cur + 1) % edits.size()]->focused = true;
}

bool PaneView::HasFocus() const {
    bool found = false;
    std::function<void(const Node*)> walk = [&](const Node* n) {
        if (n->focused) found = true;
        for (const auto& c : n->children) walk(&c);
    };
    for (const auto& ch : m_root.children) walk(&ch);
    return found;
}

const std::string& PaneView::GetText(const std::string& name) const {
    static const std::string empty;
    const Node* n = FindNode(name);
    return n ? n->text : empty;
}

void PaneView::SetText(const std::string& name, const std::string& text) {
    if (Node* n = FindNode(name))
        n->text = text;
}

void PaneView::SetVisibleByName(const std::string& name, bool visible) {
    if (Node* n = FindNode(name))
        n->visible = visible;
}

void PaneView::SetVisibleByTag(const std::string& tag, bool visible) {
    std::function<void(Node*)> walk = [&](Node* n) {
        if (n->tag == tag)
            n->visible = visible;
        for (auto& ch : n->children) walk(&ch);
    };
    for (auto& ch : m_root.children) walk(&ch);
}

void PaneView::AdjustRectByCaptionId(const std::string& captionId, int x, int y,
                                     int w, int h) {
    std::function<void(Node*)> walk = [&](Node* n) {
        if (n->captionId == captionId) {
            n->rx = x; n->ry = y;
            n->w = w;  n->h = h;
        }
        for (auto& ch : n->children) walk(&ch);
    };
    for (auto& ch : m_root.children) walk(&ch);
    // Recompute absolutes from the parent-relative rects (idempotent Layout).
    if (m_screenW > 0 && m_screenH > 0)
        Layout(m_screenW, m_screenH);
}

void PaneView::SetListItems(const std::string& name, const std::vector<std::string>& items) {
    if (Node* n = FindNode(name)) {
        n->items = items;
        if (n->selected >= (int)items.size())
            n->selected = items.empty() ? -1 : 0;
        if (n->selected < 0 && !items.empty())
            n->selected = 0;
    }
}

void PaneView::SetListItemValues(const std::string& name, const std::vector<int>& values) {
    if (Node* n = FindNode(name))
        n->itemValues = values;
}

void PaneView::SetListItemFlags(const std::string& name, const std::vector<uint32_t>& flags) {
    if (Node* n = FindNode(name))
        n->itemFlags = flags;
}

int PaneView::GetListSelection(const std::string& name) const {
    const Node* n = FindNode(name);
    return n ? n->selected : -1;
}

PaneView::Node* PaneView::FindNode(const std::string& name) {
    Node* found = nullptr;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (n->name == name) found = n;
        for (auto& ch : n->children) walk(&ch);
    };
    for (auto& ch : m_root.children) walk(&ch);
    return found;
}

const PaneView::Node* PaneView::FindNode(const std::string& name) const {
    const Node* found = nullptr;
    std::function<void(const Node*)> walk = [&](const Node* n) {
        if (n->name == name) found = n;
        for (const auto& c : n->children) walk(&c);
    };
    for (const auto& ch : m_root.children) walk(&ch);
    return found;
}

} // namespace tmx
