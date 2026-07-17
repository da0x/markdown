// inline_parser.hpp — turn a run of Markdown text into inline nodes.
#pragma once

#include <string_view>
#include <vector>

#include "ast.hpp"

namespace md {

// Parse inline Markdown (emphasis, code, links, …) into a node list.
std::vector<InlineNode> parse_inlines(std::string_view text);

}  // namespace md
