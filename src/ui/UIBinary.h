#pragma once

#include <cstdint>

namespace tmx {

// Binary UI file format — faithful port of Projects/TMProject/UIBinary.h.
// Stream layout: [CONTROL_TYPE (i32 LE)][BinXxx struct] repeated until EOF.
// Strings come from nStringIndex → g_UIString[500][64] (UI/UIString.txt,
// loaded by ReadUIString in Basedef.cpp:1172).

struct BinCheckBox {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nColor;
};

struct BinListBox {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nMaxCount;
    int32_t nVisibleCount;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nColor;
    int32_t nFillType;
    int32_t nSelect;
    int32_t nScroll;
};

struct BinGrid {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nRowCount;
    int32_t nColumnCount;
    int32_t nType;
};

struct BinPanel {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nColor;
    int32_t nFillType;
    int32_t nPickable;
};

struct Bin3DObj {
    int32_t nID;
    int32_t nParentID;
    int32_t n3DObjIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
};

struct BinButton {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nColor;
    int32_t nSound;
    int32_t nStringIndex;
};

struct BinText {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nFontColor;
    int32_t nBorder;
    int32_t nBorderColor;
    int32_t nTextType;
    int32_t nAlignType;
    int32_t nStringIndex;
};

struct BinEdit {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nFontColor;
    int32_t nBorder;
    int32_t nBorderColor;
    int32_t nTextType;
    int32_t nAlignType;
    int32_t nMaxStringLength;
    int32_t nPassword;
    char szString[128];
};

struct BinProgress {
    int32_t nID;
    int32_t nParentID;
    int32_t nTextureSetIndex;
    int32_t nCurrent;
    int32_t nMaxValue;
    int32_t nStartX;
    int32_t nStartY;
    int32_t nWidth;
    int32_t nHeight;
    int32_t nProgressColor;
    int32_t nColor;
    int32_t nStyle;
};

}
