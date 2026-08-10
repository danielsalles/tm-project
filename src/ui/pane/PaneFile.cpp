#include "pane/PaneFile.h"

#include <cstdio>
#include <cstring>
#include <sstream>

namespace tmx {

const char* PaneElement::Get(const char* key) const {
    for (auto& kv : props) {
        if (kv.first == key)
            return kv.second.c_str();
    }
    return nullptr;
}

const char* PaneFile::Get(const char* key) const {
    for (auto& kv : props) {
        if (kv.first == key)
            return kv.second.c_str();
    }
    return nullptr;
}

bool ParseArea(const char* s, int& x1, int& y1, int& x2, int& y2) {
    return s && sscanf(s, "%d %d %d %d", &x1, &y1, &x2, &y2) == 4;
}

namespace {

// Case-insensitive compare for the block keywords (parser in the client uses
// _wcsicmp: "panel"/"Panel" both valid).
bool IEq(const std::string& a, const char* b) {
    size_t n = strlen(b);
    if (a.size() != n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb)
            return false;
    }
    return true;
}

} // namespace

bool ParsePaneText(const std::string& text, PaneFile& out) {
    std::istringstream in(text);
    std::string line;

    // Parser state: -1 = before Panel, 0 = in Panel block, >0 = element depth
    PaneElement* stack[8] = {};
    int depth = 0;
    bool inPanel = false;
    bool sawPanel = false;

    auto addProp = [&](const std::string& l) {
        size_t sp = l.find_first_of(" \t");
        std::string key = sp == std::string::npos ? l : l.substr(0, sp);
        std::string val = sp == std::string::npos ? "" : l.substr(sp + 1);
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t'))
            val.erase(val.begin());
        if (depth > 0)
            stack[depth - 1]->props.emplace_back(key, val);
        else if (inPanel)
            out.props.emplace_back(key, val);
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos)
            continue;
        line = line.substr(b);
        if (line.rfind("//", 0) == 0)
            continue;

        if (line == "{")
            continue;
        if (line == "}") {
            if (depth > 0)
                --depth;
            else if (inPanel)
                inPanel = false;
            continue;
        }

        if (IEq(line, "Panel") || IEq(line, "PanelEx")) {
            sawPanel = true;
            inPanel = true;
            continue;
        }

        if (line.rfind("element", 0) == 0 || line.rfind("Element", 0) == 0) {
            // element Type  or  element Type(name)
            std::string rest = line.substr(7);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
                rest.erase(rest.begin());
            PaneElement el;
            size_t paren = rest.find('(');
            if (paren != std::string::npos) {
                el.type = rest.substr(0, paren);
                size_t close = rest.find(')', paren);
                if (close != std::string::npos)
                    el.name = rest.substr(paren + 1, close - paren - 1);
            } else {
                el.type = rest;
            }
            if (depth > 0) {
                stack[depth - 1]->children.push_back(std::move(el));
                stack[depth] = &stack[depth - 1]->children.back();
            } else {
                out.elements.push_back(std::move(el));
                stack[0] = &out.elements.back();
            }
            if (depth < 7)
                ++depth;
            continue;
        }

        addProp(line);
    }
    return sawPanel;
}

} // namespace tmx
