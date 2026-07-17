// unicode.hpp — UTF-8 decoding and terminal display-width measurement.
//
// This is the piece the olog/catcsv table code got wrong: it measured with
// byte length, so any non-ASCII cell misaligned. Here `display_width` counts
// terminal columns — wide (CJK/emoji) code points count as 2, combining and
// zero-width marks as 0.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace md {

// Decode the UTF-8 code point starting at `s[i]`, advancing `i` past it.
// Invalid bytes decode as U+FFFD and advance by one, so this never loops.
inline char32_t decode_utf8(std::string_view s, std::size_t& i) {
    auto b0 = static_cast<unsigned char>(s[i]);
    auto cont = [&](std::size_t k) -> bool {
        return k < s.size() && (static_cast<unsigned char>(s[k]) & 0xC0) == 0x80;
    };
    if (b0 < 0x80) {
        ++i;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && cont(i + 1)) {
        char32_t cp = ((b0 & 0x1F) << 6) |
                      (static_cast<unsigned char>(s[i + 1]) & 0x3F);
        i += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0 && cont(i + 1) && cont(i + 2)) {
        char32_t cp = ((b0 & 0x0F) << 12) |
                      ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[i + 2]) & 0x3F);
        i += 3;
        return cp;
    }
    if ((b0 & 0xF8) == 0xF0 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
        char32_t cp = ((b0 & 0x07) << 18) |
                      ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                      ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[i + 3]) & 0x3F);
        i += 4;
        return cp;
    }
    ++i;
    return 0xFFFD;
}

namespace detail {

inline bool in_range(char32_t cp, char32_t lo, char32_t hi) {
    return cp >= lo && cp <= hi;
}

// Zero-width: combining marks, ZWJ/ZWNJ, variation selectors, etc.
inline bool is_zero_width(char32_t cp) {
    if (cp == 0) return true;
    return in_range(cp, 0x0300, 0x036F) ||  // combining diacritical marks
           in_range(cp, 0x1AB0, 0x1AFF) ||
           in_range(cp, 0x1DC0, 0x1DFF) ||
           in_range(cp, 0x20D0, 0x20FF) ||
           in_range(cp, 0xFE00, 0xFE0F) ||  // variation selectors
           in_range(cp, 0xFE20, 0xFE2F) ||
           cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0xFEFF;
}

// Wide (2-column) code points: the common CJK, Hangul, and emoji blocks.
inline bool is_wide(char32_t cp) {
    return in_range(cp, 0x1100, 0x115F) ||   // Hangul Jamo
           in_range(cp, 0x2E80, 0x303E) ||   // CJK radicals, Kangxi
           in_range(cp, 0x3041, 0x33FF) ||   // Hiragana..CJK symbols
           in_range(cp, 0x3400, 0x4DBF) ||   // CJK Ext A
           in_range(cp, 0x4E00, 0x9FFF) ||   // CJK Unified
           in_range(cp, 0xA000, 0xA4CF) ||   // Yi
           in_range(cp, 0xAC00, 0xD7A3) ||   // Hangul syllables
           in_range(cp, 0xF900, 0xFAFF) ||   // CJK compatibility
           in_range(cp, 0xFE30, 0xFE4F) ||   // CJK compat forms
           in_range(cp, 0xFF00, 0xFF60) ||   // fullwidth forms
           in_range(cp, 0xFFE0, 0xFFE6) ||
           in_range(cp, 0x1F300, 0x1FAFF) || // emoji & pictographs
           in_range(cp, 0x20000, 0x3FFFD);   // CJK Ext B+
}

}  // namespace detail

// Terminal columns occupied by one code point.
inline int char_width(char32_t cp) {
    if (detail::is_zero_width(cp)) return 0;
    if (detail::is_wide(cp)) return 2;
    return 1;
}

// Total terminal display width of a UTF-8 string.
inline int display_width(std::string_view s) {
    int w = 0;
    std::size_t i = 0;
    while (i < s.size()) w += char_width(decode_utf8(s, i));
    return w;
}

}  // namespace md
