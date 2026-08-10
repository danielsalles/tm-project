#pragma once

#include <string>
#include <vector>

namespace tmx {

// .pane parsed representation (decrypted UTF-16 text, converted to UTF-8).
//
//   Panel
//   {
//       Resource ui/default.guimat
//       Width 331
//       ...
//   }
//   element Button(btnLogin)
//   {
//       Area 39 121 97 22
//       Caption uu_05
//   }
struct PaneElement {
    std::string type;                 // Image, Button, Static, Editbox, ...
    std::string name;                 // optional (name) after the type
    std::vector<std::pair<std::string, std::string>> props;
    std::vector<PaneElement> children;

    const char* Get(const char* key) const;
};

struct PaneFile {
    std::vector<std::pair<std::string, std::string>> props; // Panel block
    std::vector<PaneElement> elements;

    const char* Get(const char* key) const;
};

// Parses decrypted pane text. Tolerant line-based parser: skips blank lines,
// // comments, handles { } nesting and "element Type(name)" headers.
bool ParsePaneText(const std::string& text, PaneFile& out);

// Helpers for common property shapes.
bool ParseArea(const char* s, int& x1, int& y1, int& x2, int& y2);

} // namespace tmx
