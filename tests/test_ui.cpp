// test_ui.cpp — headless tests for the UI system (no GL context needed).
#include "test_framework.h"
#include "ui/UITypes.h"
#include "ui/SControl.h"
#include "ui/SControls.h"
#include "ui/SControlContainer.h"
#include "ui/UIBinary.h"
#include "ui/UILoader.h"

#include <cstring>

using namespace tmx;

// --- UITypes tests ---
TEST(ui, PointInRect_basic) {
    EXPECT_TRUE(PointInRect(5, 5, 0, 0, 10, 10) == 1);
    EXPECT_TRUE(PointInRect(0, 0, 0, 0, 10, 10) == 1);
    EXPECT_TRUE(PointInRect(10, 10, 0, 0, 10, 10) == 0); // edge exclusive
    EXPECT_TRUE(PointInRect(11, 5, 0, 0, 10, 10) == 0);
    EXPECT_TRUE(PointInRect(5, 11, 0, 0, 10, 10) == 0);
}

TEST(ui, IVector2_defaults) {
    IVector2 v;
    EXPECT_EQ(v.x, 0);
    EXPECT_EQ(v.y, 0);
    IVector2 v2(10, 20);
    EXPECT_EQ(v2.x, 10);
    EXPECT_EQ(v2.y, 20);
}

// --- SControl tests ---
TEST(ui, SControl_FindControl) {
    SControl root;
    root.m_dwControlID = 1;

    SControl child1;
    child1.m_dwControlID = 2;
    root.AddChild(&child1);

    SControl child2;
    child2.m_dwControlID = 3;
    root.AddChild(&child2);

    SControl grandchild;
    grandchild.m_dwControlID = 4;
    child1.AddChild(&grandchild);

    EXPECT_TRUE(root.FindControl(1) == &root);
    EXPECT_TRUE(root.FindControl(2) == &child1);
    EXPECT_TRUE(root.FindControl(3) == &child2);
    EXPECT_TRUE(root.FindControl(4) == &grandchild);
    EXPECT_TRUE(root.FindControl(99) == nullptr);
}

TEST(ui, SControl_PtInControl) {
    SControl ctrl;
    ctrl.m_nPosX = 10; ctrl.m_nPosY = 20;
    ctrl.m_nWidth = 100; ctrl.m_nHeight = 50;

    EXPECT_TRUE(ctrl.PtInControl(50, 30) == 1);
    EXPECT_TRUE(ctrl.PtInControl(5, 25) == 0);
    EXPECT_TRUE(ctrl.PtInControl(15, 25) == 1);
}

// --- SPanel tests ---
TEST(ui, SPanel_construction) {
    SPanel panel(1, 10, 20, 100, 50, 0xFFFFFFFF, RENDER_IMAGE_STRETCH);
    EXPECT_TRUE(panel.m_eCtrlType == CTRL_TYPE_PANEL);
    EXPECT_TRUE(panel.m_GCPanel.nTextureSetIndex == 1);
    EXPECT_TRUE(panel.m_nWidth == 100);
}

TEST(ui, SPanel_FrameMove2_skip_invisible) {
    SPanel panel;
    panel.m_bVisible = 0;
    stGeomList drawList[30] = {};
    IVector2 pos = {0, 0};
    panel.FrameMove2(drawList, pos, 0, 0);
    EXPECT_TRUE(drawList[0].pHeadGeom == nullptr);
}

// --- SButton tests ---
TEST(ui, SButton_state_transitions) {
    SButton btn(1, 0, 0, 50, 20, 0xFFFFFFFF, 0, "OK");
    EXPECT_TRUE(btn.m_eCtrlType == CTRL_TYPE_BUTTON);
    EXPECT_TRUE(btn.m_bPressed == 0);

    // Mouse over
    btn.OnMouseEvent(WM_MOUSEMOVE, 0, 25, 10);
    EXPECT_TRUE(btn.m_bOver == 1);
    EXPECT_TRUE(btn.m_bMouseOver == 1);

    // Press
    btn.OnMouseEvent(WM_LBUTTONDOWN, 1, 25, 10);
    EXPECT_TRUE(btn.m_bPressed == 1);

    // Release
    btn.OnMouseEvent(WM_LBUTTONUP, 0, 25, 10);
    EXPECT_TRUE(btn.m_bPressed == 0);
}

// --- SText tests ---
TEST(ui, SText_construction) {
    SText txt(1, "Hello", 0xFF00FF00, 10, 20, 200, 30, 0, 0, 0, 0);
    EXPECT_TRUE(txt.m_eCtrlType == CTRL_TYPE_TEXT);
    EXPECT_TRUE(strcmp(txt.m_szString, "Hello") == 0);
    EXPECT_TRUE(txt.m_dwColor == 0xFF00FF00);
}

// --- SEditableText tests ---
TEST(ui, SEditableText_typing) {
    SEditableText edit;
    edit.m_bFocused = 1;
    edit.m_nMaxLen = 10;

    edit.OnCharEvent('H');
    edit.OnCharEvent('i');
    EXPECT_TRUE(strcmp(edit.m_szString, "Hi") == 0);
    EXPECT_TRUE(edit.m_nCursorPos == 2);

    edit.OnCharEvent(8); // backspace
    EXPECT_TRUE(strcmp(edit.m_szString, "H") == 0);
    EXPECT_TRUE(edit.m_nCursorPos == 1);
}

// --- SListBox tests ---
TEST(ui, SListBox_add_remove) {
    SListBox list;
    EXPECT_TRUE(list.AddItem("Item1") == 0);
    EXPECT_TRUE(list.AddItem("Item2") == 1);
    EXPECT_TRUE(list.AddItem("Item3") == 2);
    EXPECT_TRUE(list.m_nItemCount == 3);

    list.RemoveItem(1);
    EXPECT_TRUE(list.m_nItemCount == 2);
    EXPECT_TRUE(strcmp(list.m_items[0].str, "Item1") == 0);
    EXPECT_TRUE(strcmp(list.m_items[1].str, "Item3") == 0);

    list.RemoveAll();
    EXPECT_TRUE(list.m_nItemCount == 0);
}

// --- SCheckBox tests ---
TEST(ui, SCheckBox_toggle) {
    SCheckBox cb;
    cb.m_bOver = 1;
    cb.m_bVisible = 1;

    cb.OnMouseEvent(WM_LBUTTONUP, 0, 5, 5);
    EXPECT_TRUE(cb.m_bChecked == 1);

    cb.OnMouseEvent(WM_LBUTTONUP, 0, 5, 5);
    EXPECT_TRUE(cb.m_bChecked == 0);
}

// --- SMessageBox tests ---
TEST(ui, SMessageBox_key_events) {
    SMessageBox mb;
    mb.m_bFocused = 1;

    // 'Y' should fire event 0 (OK)
    int result = mb.OnCharEvent('Y');
    EXPECT_TRUE(result == 1);

    // 'N' should fire event 1 (Cancel)
    result = mb.OnCharEvent('N');
    EXPECT_TRUE(result == 1);
}

// --- SControlContainer tests ---
TEST(ui, SControlContainer_tree) {
    SControlContainer container;
    EXPECT_TRUE(container.FindControl(0) != nullptr); // root
    EXPECT_TRUE(container.FindControl(999) == nullptr);
}

TEST(ui, SControlContainer_FrameMove_empty) {
    SControlContainer container;
    container.FrameMove(0);
    for (int i = 0; i < MAX_DRAW_CONTROL; ++i) {
        EXPECT_TRUE(container.m_pDrawControl[i].pHeadGeom == nullptr);
    }
}

// --- AddRenderControlItem tests ---
TEST(ui, AddRenderControlItem_basic) {
    stGeomList list[30] = {};
    GeomControl ctrl;
    ctrl.eRenderType = RENDER_IMAGE;

    int result = AddRenderControlItem(list, &ctrl, 5);
    EXPECT_TRUE(result == 1);
    EXPECT_TRUE(list[5].pHeadGeom == &ctrl);
    EXPECT_TRUE(list[5].pTailGeom == &ctrl);

    GeomControl ctrl2;
    ctrl2.eRenderType = RENDER_TEXT;
    AddRenderControlItem(list, &ctrl2, 5);
    EXPECT_TRUE(list[5].pHeadGeom == &ctrl);
    EXPECT_TRUE(list[5].pTailGeom == &ctrl2);
    EXPECT_TRUE(ctrl.m_pNextGeom == &ctrl2);
}

TEST(ui, AddRenderControlItem_bounds) {
    stGeomList list[30] = {};
    GeomControl ctrl;
    EXPECT_TRUE(AddRenderControlItem(list, &ctrl, -1) == 0);
    EXPECT_TRUE(AddRenderControlItem(list, &ctrl, 30) == 0);
}

// --- RemoveRenderControlItem tests ---
TEST(ui, RemoveRenderControlItem) {
    stGeomList list[30] = {};
    GeomControl c1, c2, c3;
    AddRenderControlItem(list, &c1, 3);
    AddRenderControlItem(list, &c2, 3);
    AddRenderControlItem(list, &c3, 3);

    RemoveRenderControlItem(list, &c2, 3);
    EXPECT_TRUE(list[3].pHeadGeom == &c1);
    EXPECT_TRUE(list[3].pTailGeom == &c3);
    EXPECT_TRUE(c1.m_pNextGeom == &c3);
}

// --- SGridControl tests ---
TEST(ui, SGridControl_construction) {
    SGridControl grid(1, 3, 4, 0, 0, 120, 80, 0);
    EXPECT_TRUE(grid.m_nRows == 3);
    EXPECT_TRUE(grid.m_nCols == 4);
    EXPECT_TRUE(grid.m_eCtrlType == CTRL_TYPE_GRID);
}

// --- UIBinary / UILoader tests ---
TEST(ui, UILoader_ReadRCBin_panel) {
    // Stream: [type=PANEL][BinPanel 40B] — matches the real file layout.
    uint8_t data[64] = {};
    int32_t type = 1;  // CTRL_TYPE_PANEL
    memcpy(data, &type, 4);
    BinPanel b{};
    b.nID = 311; b.nParentID = 0; b.nTextureSetIndex = 12;
    b.nStartX = 0; b.nStartY = 0; b.nWidth = 144; b.nHeight = 256;
    b.nColor = -1; b.nFillType = 4; b.nPickable = 1;
    memcpy(data + 4, &b, sizeof(b));

    SControlContainer container;
    std::string err;
    EXPECT_TRUE(UILoader::ReadRCBin(data, 4 + sizeof(b), container, &err));
    SControl* found = container.FindControl(311);
    EXPECT_TRUE(found != nullptr);
    if (found) {
        EXPECT_TRUE(found->m_eCtrlType == CTRL_TYPE_PANEL);
        EXPECT_TRUE(found->m_nWidth == 144.0f);
        EXPECT_TRUE(((SPanel*)found)->m_bPickable == 1);
    }
}

TEST(ui, UILoader_ReadRCBin_parent_child) {
    // Panel (id=10) + Button (id=11, parent=10)
    uint8_t data[128] = {};
    int32_t type = 1;
    memcpy(data, &type, 4);
    BinPanel b{};
    b.nID = 10; b.nTextureSetIndex = 1;
    b.nWidth = 200; b.nHeight = 100; b.nColor = -1; b.nFillType = 4;
    memcpy(data + 4, &b, sizeof(b));
    size_t off = 4 + sizeof(b);

    int32_t type2 = 2;  // CTRL_TYPE_BUTTON
    memcpy(data + off, &type2, 4);
    BinButton bb{};
    bb.nID = 11; bb.nParentID = 10; bb.nTextureSetIndex = 1;
    bb.nWidth = 50; bb.nHeight = 20; bb.nColor = -1;
    memcpy(data + off + 4, &bb, sizeof(bb));

    SControlContainer container;
    std::string err;
    EXPECT_TRUE(UILoader::ReadRCBin(data, off + 4 + sizeof(bb), container, &err));
    SControl* parent = container.FindControl(10);
    SControl* child = container.FindControl(11);
    EXPECT_TRUE(parent != nullptr);
    EXPECT_TRUE(child != nullptr);
    if (parent && child) {
        EXPECT_TRUE(child->m_pParent == parent);
        EXPECT_TRUE(parent->m_pDown == child);
    }
}

TEST(ui, UILoader_truncated_stream_fails) {
    uint8_t data[8] = {};
    int32_t type = 1;
    memcpy(data, &type, 4);  // PANEL but no struct data
    SControlContainer container;
    std::string err;
    EXPECT_FALSE(UILoader::ReadRCBin(data, sizeof(data), container, &err));
}

TEST(ui, UILoader_UIString) {
    const char* txt = "1 Hello\n2 World\n";
    EXPECT_TRUE(UILoader::LoadUIStrings(txt, strlen(txt)));
    EXPECT_TRUE(strcmp(UILoader::UIString(1), "Hello") == 0);
    EXPECT_TRUE(strcmp(UILoader::UIString(2), "World") == 0);
    EXPECT_TRUE(strcmp(UILoader::UIString(999), "") == 0);  // out of range
}
