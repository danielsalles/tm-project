// test_pane.cpp — Phase 8d: .pane system (crypto, guimat, parser, view flow).
#include "test_framework.h"
#include "pane/PaneCrypto.h"
#include "pane/GuiMat.h"
#include "pane/PaneFile.h"
#include "pane/PaneView.h"

#include <cstring>
#include <string>

using namespace tmx;

// Minimal delta-cipher check: encrypt a known plaintext, then decrypt.
static std::vector<uint8_t> PaneEncryptForTest(const uint8_t* plain, size_t n) {
    // inverse of: buf[i] += 14*(i/14) - buf[i-1] - i ; buf[0] -= 14
    std::vector<uint8_t> c(plain, plain + n);
    c[0] = (uint8_t)(c[0] + 14);
    for (size_t i = 1; i < n; ++i)
        c[i] = (uint8_t)(c[i] - 14 * (i / 14) + c[i - 1] + (i & 0xFF));
    return c;
}

TEST(pane, crypto_roundtrip) {
    const char* plain = "Panel\r\n{\r\n  Width 331\r\n}";
    auto enc = PaneEncryptForTest((const uint8_t*)plain, strlen(plain));
    auto dec = PaneDecrypt(enc.data(), enc.size());
    EXPECT_TRUE(dec.size() == strlen(plain));
    EXPECT_TRUE(memcmp(dec.data(), plain, dec.size()) == 0);
}

TEST(pane, crypto_real_file_header) {
    // ui/Panel/login/GameLogin.pane decrypts to UTF-16LE "Panel"
    FILE* f = fopen((std::string(TM_REPO_ROOT) + "/ui/Panel/login/GameLogin.pane").c_str(), "rb");
    EXPECT_TRUE(f != nullptr);
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes((size_t)sz);
    if (fread(bytes.data(), 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return; }
    fclose(f);
    std::string text = PaneDecryptToUtf8(bytes.data(), bytes.size());
    EXPECT_TRUE(text.rfind("Panel", 0) == 0);
    EXPECT_TRUE(text.find("element Button(btnSelServer)") != std::string::npos);
    EXPECT_TRUE(text.find("element Button(btnExit)") != std::string::npos);
}

TEST(pane, guimat_parse) {
    std::string text =
        "FontList\n{\n\t0,0,10,0,false,false\n\t2,0,12,0,false,false\n}\n"
        "TextureList\n{\n\t3 0 NUI/ServerList.wyt\n\t6 -16777216 NUI/main.wyt\n}\n"
        "TextureAlias\n{\n\tUI.ButtonN <6> 539 101 596 118\n\tUI_Login.ServerBG <3> 0 4 144 258\n}\n";
    GuiMat gm;
    EXPECT_TRUE(gm.Parse(text));
    EXPECT_TRUE(gm.FontHeight(2) == 12);
    const GuiMat::Texture* t = gm.FindTexture(6);
    EXPECT_TRUE(t && t->path == "NUI/main.wyt");
    const GuiMat::Alias* a = gm.FindAlias("UI.ButtonN");
    EXPECT_TRUE(a && a->texIndex == 6 && a->Width() == 57 && a->Height() == 17);
    EXPECT_TRUE(gm.FindAlias("UI_Login.ServerBG") != nullptr);
    EXPECT_TRUE(gm.FindAlias("nonexistent") == nullptr);
}

TEST(pane, parse_panel_and_elements) {
    std::string text =
        "Panel\n{\n\tResource ui/default.guimat\n\tAlignPanelX Center\n"
        "\tWidth 331\n\tHeight 280\n}\n"
        "element Button(btnLogin)\n{\n\tArea 39 121 97 22\n\tCaption uu_05\n}\n"
        "element Static\n{\n\tArea 19 60 87 16\n\tFontAlign center vcenter\n\tCaption uu_08\n}\n";
    PaneFile pf;
    EXPECT_TRUE(ParsePaneText(text, pf));
    EXPECT_TRUE(pf.elements.size() == 2);
    EXPECT_TRUE(pf.Get("Width") && !strcmp(pf.Get("Width"), "331"));
    const PaneElement& btn = pf.elements[0];
    EXPECT_TRUE(btn.type == "Button");
    EXPECT_TRUE(btn.name == "btnLogin");
    int x, y, w, h;
    EXPECT_TRUE(ParseArea(btn.Get("Area"), x, y, w, h));
    EXPECT_TRUE(x == 39 && y == 121 && w == 97 && h == 22);  // Area = x y w h
    EXPECT_TRUE(pf.elements[1].type == "Static");
    EXPECT_TRUE(!strcmp(pf.elements[1].Get("Caption"), "uu_08"));
}

TEST(pane, view_layout_and_click) {
    // Real GameLogin.pane: layout at 1024x768, click Conectar → btnSelServer.
    FILE* f = fopen((std::string(TM_REPO_ROOT) + "/ui/Panel/login/GameLogin.pane").c_str(), "rb");
    EXPECT_TRUE(f != nullptr);
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes((size_t)sz);
    if (fread(bytes.data(), 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return; }
    fclose(f);

    PaneFile pf;
    EXPECT_TRUE(ParsePaneText(PaneDecryptToUtf8(bytes.data(), bytes.size()), pf));

    PaneView view;
    EXPECT_TRUE(view.Load(std::move(pf), nullptr));  // no guimat: no art, logic only
    view.Layout(1024, 768);

    // Panel 331x280 centered, OffsetY 50 → x=346, y=(768-280)/2+50=294
    EXPECT_TRUE(view.X() == 346);
    EXPECT_TRUE(view.Y() == 294);

    // btnSelServer at panel+(28,240) 102x23 → center (425, 545)
    EXPECT_TRUE(view.OnMouseDown(425, 545));
    EXPECT_TRUE(view.OnMouseUp(425, 545));
    EXPECT_TRUE(view.LastClick() == "btnSelServer");

    // Layout must be idempotent (resize re-layout in the app double-offset
    // children before this regression test existed)
    view.Layout(1024, 768);
    view.ClearClick();
    EXPECT_TRUE(view.OnMouseDown(425, 545));
    EXPECT_TRUE(view.OnMouseUp(425, 545));
    EXPECT_TRUE(view.LastClick() == "btnSelServer");

    // btnExit at panel+(176,240) 119x23 → center ~(581, 545)
    view.ClearClick();
    EXPECT_TRUE(view.OnMouseDown(581, 545));
    EXPECT_TRUE(view.OnMouseUp(581, 545));
    EXPECT_TRUE(view.LastClick() == "btnExit");
}

TEST(pane, view_edit_focus_and_typing) {
    std::string text =
        "Panel\n{\n\tWidth 271\n\tHeight 192\n}\n"
        "element ImeEditbox(id)\n{\n\tArea 118 57 120 22\n}\n"
        "element Editbox(password)\n{\n\tArea 118 84 120 20\n\tPassword true\n}\n";
    PaneFile pf;
    EXPECT_TRUE(ParsePaneText(text, pf));
    PaneView view;
    view.Load(std::move(pf), nullptr);
    view.Layout(1024, 768);

    // click the id field → focus, type, TAB to password
    EXPECT_TRUE(view.OnMouseDown(view.X() + 120, view.Y() + 60));
    EXPECT_TRUE(view.HasFocus());
    view.OnChar('a'); view.OnChar('b');
    EXPECT_TRUE(view.GetText("id") == "ab");
    view.FocusNext();
    view.OnChar('x');
    EXPECT_TRUE(view.GetText("password") == "x");
    view.Backspace();
    EXPECT_TRUE(view.GetText("password").empty());
    view.FocusNext();  // wraps to id
    view.OnChar('c');
    EXPECT_TRUE(view.GetText("id") == "abc");
}
