// renderer.hpp — render a block tree to an ANSI-styled, width-wrapped string.
#pragma once

#include <string>
#include <vector>

#include "ast.hpp"

namespace md {

// Render top-level blocks to a printable string (no trailing newline).
std::string render_document(const std::vector<BlockNode>& blocks, int width);

}  // namespace md
