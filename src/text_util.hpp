// text_util.hpp — width and truncation over strings that already contain ANSI
// escape sequences. Escapes take zero columns and must never be split.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "ansi.hpp"
#include "unicode.hpp"

namespace md {

// Advance `i` (positioned at ESC) past a CSI (…m) or OSC (…ST/BEL) sequence.
inline void skip_escape(std::string_view s, std::size_t& i) {
    ++i;  // past ESC
    if (i >= s.size()) return;
    char c = s[i];
    if (c == '[') {  // CSI: ends at a final byte 0x40–0x7E
        ++i;
        while (i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7E)) ++i;
        if (i < s.size()) ++i;
    } else if (c == ']') {  // OSC: ends at BEL or ST (ESC \)
        ++i;
        while (i < s.size()) {
            if (s[i] == 0x07) {
                ++i;
                break;
            }
            if (s[i] == 0x1B && i + 1 < s.size() && s[i + 1] == '\\') {
                i += 2;
                break;
            }
            ++i;
        }
    } else {
        ++i;
    }
}

// Display width of a string, ignoring ANSI escapes.
inline int visible_width(std::string_view s) {
    int w = 0;
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == 0x1B)
            skip_escape(s, i);
        else
            w += char_width(decode_utf8(s, i));
    }
    return w;
}

// Truncate to at most `max_width` display columns, appending an ellipsis when
// anything was cut. Escape sequences are copied whole and a reset is appended so
// styling never bleeds past the cut.
inline std::string truncate_ansi(std::string_view s, int max_width) {
    if (visible_width(s) <= max_width) return std::string(s);
    if (max_width <= 0) return {};
    int budget = max_width - 1;  // leave a column for the ellipsis
    std::string out;
    int w = 0;
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == 0x1B) {
            std::size_t j = i;
            skip_escape(s, j);
            out.append(s.substr(i, j - i));
            i = j;
        } else {
            std::size_t j = i;
            int cw = char_width(decode_utf8(s, j));
            if (w + cw > budget) break;
            out.append(s.substr(i, j - i));
            w += cw;
            i = j;
        }
    }
    out += "…";
    if (g_color_enabled) out += std::string(kReset);
    return out;
}

}  // namespace md
