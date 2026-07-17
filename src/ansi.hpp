// ansi.hpp — ANSI SGR styling primitives.
//
// A `Style` is a small bag of visual attributes. `sgr()` turns one into an
// escape sequence; when color is globally disabled (piped output, --no-color)
// every helper collapses to empty strings so the plain text falls through
// untouched.
#pragma once

#include <string>

namespace md {

// Global switch flipped by main() based on isatty()/flags. When false, no
// escape codes are ever emitted.
inline bool g_color_enabled = true;

// Basic 8/16-color palette (foreground). -1 means "no explicit color".
enum class Color : int {
    None = -1,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97,
};

struct Style {
    bool bold = false;
    bool italic = false;
    bool dim = false;
    bool underline = false;
    bool strike = false;
    Color fg = Color::None;

    bool empty() const {
        return !bold && !italic && !dim && !underline && !strike &&
               fg == Color::None;
    }
};

inline constexpr std::string_view kReset = "\x1b[0m";

// Build the SGR escape sequence that turns `s` on. Returns "" when the style is
// empty or color is disabled.
inline std::string sgr(const Style& s) {
    if (!g_color_enabled || s.empty()) return {};
    std::string out = "\x1b[";
    bool first = true;
    auto add = [&](int code) {
        if (!first) out += ';';
        out += std::to_string(code);
        first = false;
    };
    if (s.bold) add(1);
    if (s.dim) add(2);
    if (s.italic) add(3);
    if (s.underline) add(4);
    if (s.strike) add(9);
    if (s.fg != Color::None) add(static_cast<int>(s.fg));
    out += 'm';
    return out;
}

// Wrap `text` in the style's on/off codes. No-op wrapping when disabled.
inline std::string styled(const std::string& text, const Style& s) {
    std::string on = sgr(s);
    if (on.empty()) return text;
    return on + text + std::string(kReset);
}

// Emit an OSC 8 clickable hyperlink around `text`. Terminals that don't
// understand OSC 8 silently drop the sequences and show `text` only.
inline std::string osc8(const std::string& url, const std::string& text) {
    if (!g_color_enabled || url.empty()) return text;
    return "\x1b]8;;" + url + "\x1b\\" + text + "\x1b]8;;\x1b\\";
}

}  // namespace md
