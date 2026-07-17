// block_parser.hpp — turn Markdown source into a block tree.
#pragma once

#include <string_view>
#include <vector>

#include "ast.hpp"

namespace md {

// Parse a whole document. Returns the Document's top-level blocks.
std::vector<BlockNode> parse_document(std::string_view source);

}  // namespace md
