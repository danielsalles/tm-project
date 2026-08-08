#include "ui/UILoader.h"

#include "ui/SControlContainer.h"
#include "ui/SControls.h"
#include "ui/UIBinary.h"
#include "platform/Platform.h"

#include <cstring>
#include <cstdio>

namespace tmx {

char UILoader::s_strings[UI_STRING_COUNT][UI_STRING_LEN] = {};
bool UILoader::s_stringsLoaded = false;

bool UILoader::LoadUIStrings(const char* textData, size_t size) {
    if (!textData || size == 0)
        return false;
    std::string content(textData, size);
    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\n', pos);
        std::string line = (end == std::string::npos)
            ? content.substr(pos) : content.substr(pos, end - pos);
        pos = (end == std::string::npos) ? content.size() : end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        int index = 0;
        char part[UI_STRING_LEN] = {};
        if (sscanf(line.c_str(), "%d %63s", &index, part) != 2)
            continue;
        if (index >= 0 && index < UI_STRING_COUNT)
            strncpy(s_strings[index], part, UI_STRING_LEN - 1);
    }
    s_stringsLoaded = true;
    return true;
}

const char* UILoader::UIString(int index) {
    if (index < 0 || index >= UI_STRING_COUNT || !s_stringsLoaded)
        return "";
    return s_strings[index];
}

// Reads one little-endian i32 from the stream.
static bool ReadI32(const uint8_t*& p, const uint8_t* end, int32_t& out) {
    if (p + 4 > end) return false;
    out = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
    p += 4;
    return true;
}

// Reads a fixed-size raw struct (all fields are i32 LE in the file).
static bool ReadStruct(const uint8_t*& p, const uint8_t* end, void* out, size_t bytes) {
    if (p + bytes > end) return false;
    memcpy(out, p, bytes);
    p += bytes;
    return true;
}

bool UILoader::ReadRCBin(const uint8_t* data, size_t size,
                         SControlContainer& container, std::string* err) {
    const uint8_t* p = data;
    const uint8_t* end = data + size;

    auto linkControl = [&](SControl* ctrl, int32_t id, int32_t parentID) {
        ctrl->SetControlID((uint32_t)id);
        if (parentID) {
            SControl* parent = container.FindControl((uint32_t)parentID);
            if (parent)
                parent->AddChild(ctrl);
            else
                container.AddItem(ctrl);
        } else {
            container.AddItem(ctrl);
        }
    };

    while (p < end) {
        int32_t type = 0;
        if (!ReadI32(p, end, type))
            break;

        switch ((CONTROL_TYPE)type) {
        case CTRL_TYPE_PANEL: {
            BinPanel b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SPanel(b.nTextureSetIndex, (float)b.nStartX, (float)b.nStartY,
                                 (float)b.nWidth, (float)b.nHeight, (uint32_t)b.nColor,
                                 b.nFillType);
            linkControl(c, b.nID, b.nParentID);
            c->SetCenterPos((uint32_t)b.nID, (float)b.nStartX, (float)b.nStartY,
                            (float)b.nWidth, (float)b.nHeight);
            c->m_bPickable = b.nPickable;
            break;
        }
        case CTRL_TYPE_GRID: {
            BinGrid b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SGridControl(b.nTextureSetIndex, b.nRowCount, b.nColumnCount,
                                       (float)b.nStartX, (float)b.nStartY,
                                       (float)b.nWidth, (float)b.nHeight, b.nType);
            c->SetEventListener(&container);
            linkControl(c, b.nID, b.nParentID);
            break;
        }
        case CTRL_TYPE_3DOBJ: {
            // RENDER_3DOBJ is Phase 7 — skip the record so the stream stays aligned.
            Bin3DObj b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            Log("UILoader: CTRL_TYPE_3DOBJ id=%d ignorado (Fase 7)", b.nID);
            break;
        }
        case CTRL_TYPE_BUTTON: {
            BinButton b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SButton(b.nTextureSetIndex, (float)b.nStartX, (float)b.nStartY,
                                  (float)b.nWidth, (float)b.nHeight, (uint32_t)b.nColor,
                                  b.nSound, UIString(b.nStringIndex));
            c->SetEventListener(&container);
            linkControl(c, b.nID, b.nParentID);
            c->SetCenterPos((uint32_t)b.nID, (float)b.nStartX, (float)b.nStartY,
                            (float)b.nWidth, (float)b.nHeight);
            break;
        }
        case CTRL_TYPE_TEXT: {
            BinText b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SText(b.nTextureSetIndex, UIString(b.nStringIndex),
                                (uint32_t)b.nFontColor,
                                (float)b.nStartX, (float)b.nStartY,
                                (float)b.nWidth, (float)b.nHeight,
                                (uint32_t)b.nBorderColor, b.nBorder,
                                b.nTextType, b.nAlignType);
            linkControl(c, b.nID, b.nParentID);
            c->SetCenterPos((uint32_t)b.nID, (float)b.nStartX, (float)b.nStartY,
                            (float)b.nWidth, (float)b.nHeight);
            break;
        }
        case CTRL_TYPE_EDITABLETEXT: {
            BinEdit b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            b.szString[127] = '\0';
            auto* c = new SEditableText(b.nTextureSetIndex, b.szString,
                                        b.nMaxStringLength, b.nPassword,
                                        (uint32_t)b.nFontColor,
                                        (float)b.nStartX, (float)b.nStartY,
                                        (float)b.nWidth, (float)b.nHeight);
            c->SetEventListener(&container);
            linkControl(c, b.nID, b.nParentID);
            c->SetCenterPos((uint32_t)b.nID, (float)b.nStartX, (float)b.nStartY,
                            (float)b.nWidth, (float)b.nHeight);
            break;
        }
        case CTRL_TYPE_PROGRESSBAR: {
            BinProgress b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SProgressBar(b.nTextureSetIndex, b.nCurrent, b.nMaxValue,
                                       (float)b.nStartX, (float)b.nStartY,
                                       (float)b.nWidth, (float)b.nHeight,
                                       (uint32_t)b.nProgressColor, (uint32_t)b.nColor,
                                       b.nStyle);
            c->SetEventListener(&container);
            linkControl(c, b.nID, b.nParentID);
            break;
        }
        case CTRL_TYPE_CHECKBOX: {
            BinCheckBox b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SCheckBox(b.nTextureSetIndex, (float)b.nStartX, (float)b.nStartY,
                                    (float)b.nWidth, (float)b.nHeight, (uint32_t)b.nColor);
            c->SetEventListener(&container);
            linkControl(c, b.nID, b.nParentID);
            break;
        }
        case CTRL_TYPE_LISTBOX: {
            BinListBox b{};
            if (!ReadStruct(p, end, &b, sizeof(b))) goto fail;
            auto* c = new SListBox(b.nTextureSetIndex, b.nMaxCount, b.nVisibleCount,
                                   (float)b.nStartX, (float)b.nStartY,
                                   (float)b.nWidth, (float)b.nHeight,
                                   (uint32_t)b.nColor, b.nFillType, b.nSelect, b.nScroll);
            c->SetEventListener(&container);
            linkControl(c, b.nID, b.nParentID);
            break;
        }
        default:
            Log("UILoader: tipo desconhecido %d — stream possivelmente desalinhado", type);
            goto fail;
        }
    }
    return true;

fail:
    if (err) *err = "ReadRCBin: truncated or misaligned stream";
    return false;
}

}
