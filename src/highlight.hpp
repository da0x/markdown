// highlight.hpp — lightweight syntax highlighting for fenced code blocks.
//
// A small state-machine lexer, driven by a per-language spec (comment markers,
// string delimiters, keyword sets). It is deliberately approximate: good enough
// to make code readable, not a compiler front-end. Unknown languages still get
// generic string/number highlighting.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace md {

// Highlight already-split code `lines` for `lang` (the fence info word).
// Returns one styled string per input line. When color is disabled the lines
// are returned unchanged.
std::vector<std::string> highlight_code(const std::vector<std::string>& lines,
                                        std::string_view lang);

}  // namespace md
