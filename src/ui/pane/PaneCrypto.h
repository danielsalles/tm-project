#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tmx {

// .pane file decryption (WYD 769.2 client, sub_407530):
//   for i = size-1 .. 1:  buf[i] += 14*(i/14) - buf[i-1] - i
//   buf[0] -= 14
// applied back-to-front so buf[i-1] is still ciphertext when read.
// The plaintext is UTF-16LE text (with BOM).
inline std::vector<uint8_t> PaneDecrypt(const uint8_t* data, size_t size) {
    std::vector<uint8_t> buf(data, data + size);
    if (size == 0)
        return buf;
    for (size_t i = size - 1; i > 0; --i)
        buf[i] = (uint8_t)(buf[i] + 14 * (i / 14) - buf[i - 1] - (i & 0xFF));
    buf[0] = (uint8_t)(buf[0] - 14);
    return buf;
}

// UTF-16LE → UTF-8 (with optional BOM). Used for .guimat (plaintext) and
// .pane (after decryption).
inline std::string Utf16LeToUtf8(const uint8_t* raw, size_t size) {
    std::string out;
    if (size < 2)
        return out;
    size_t i = 0;
    if (raw[0] == 0xFF && raw[1] == 0xFE)
        i = 2;
    for (; i + 1 < size; i += 2) {
        uint32_t cp = (uint32_t)raw[i] | ((uint32_t)raw[i + 1] << 8);
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// Decrypt + convert UTF-16LE → UTF-8.
inline std::string PaneDecryptToUtf8(const uint8_t* data, size_t size) {
    std::vector<uint8_t> raw = PaneDecrypt(data, size);
    return Utf16LeToUtf8(raw.data(), raw.size());
}

} // namespace tmx
