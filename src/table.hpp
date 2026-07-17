// table.hpp — box-drawn tables, following catcsv/olog's "style is data" idea:
// a border style is just a bag of glyphs, and one generator emits each line
// type. Unlike olog we measure with display_width, align per the GFM delimiter
// row, and shrink columns to fit the terminal.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "ast.hpp"
#include "text_util.hpp"

namespace md {

struct BorderStyle {
    // Corner/junction glyphs for the three horizontal rules.
    std::string tl, tj, tr;  // top:       ╭ ┬ ╮
    std::string ml, mj, mr;  // separator: ├ ┼ ┤
    std::string bl, bj, br;  // bottom:    ╰ ┴ ╯
    std::string h;           // horizontal fill: ─
    std::string v;           // vertical:        │
};

inline const BorderStyle kRounded{
    "╭", "┬", "╮", "├", "┼", "┤", "╰", "┴", "╯", "─", "│",
};

namespace detail {

constexpr int kMinCol = 3;  // never shrink a column below this many columns

// A horizontal rule spanning all columns: left + fill·(w+2) + junction + … + right.
inline std::string rule(const BorderStyle& s, const std::string& left,
                        const std::string& junction, const std::string& right,
                        const std::vector<int>& widths) {
    std::string out = left;
    for (std::size_t c = 0; c < widths.size(); ++c) {
        for (int i = 0; i < widths[c] + 2; ++i) out += s.h;
        out += (c + 1 == widths.size()) ? right : junction;
    }
    return out;
}

// Pad one already-styled cell to `width` columns per its alignment.
inline std::string pad(const std::string& cell, int width, Align align) {
    int fill = width - visible_width(cell);
    if (fill <= 0) return cell;
    switch (align) {
        case Align::Right:
            return std::string(fill, ' ') + cell;
        case Align::Center: {
            int l = fill / 2, r = fill - l;
            return std::string(l, ' ') + cell + std::string(r, ' ');
        }
        case Align::Left:
        default:
            return cell + std::string(fill, ' ');
    }
}

}  // namespace detail

// Render a table. `cells[0]` is the (already bold-styled) header row; every cell
// string may contain ANSI. `max_width` bounds the whole rendered width.
inline std::string render_table(const std::vector<std::vector<std::string>>& cells,
                                const std::vector<Align>& aligns, int max_width,
                                const BorderStyle& style = kRounded) {
    if (cells.empty()) return {};
    std::size_t cols = 0;
    for (const auto& row : cells) cols = std::max(cols, row.size());
    if (cols == 0) return {};

    std::vector<Align> al = aligns;
    al.resize(cols, Align::Left);

    // Natural column widths = widest cell in each column.
    std::vector<int> widths(cols, detail::kMinCol);
    for (const auto& row : cells)
        for (std::size_t c = 0; c < row.size(); ++c)
            widths[c] = std::max(widths[c], visible_width(row[c]));

    // Total = content + 2 pad spaces per column + one border per column edge.
    auto total_width = [&] {
        int sum = static_cast<int>(cols) + 1;  // vertical borders
        for (int w : widths) sum += w + 2;
        return sum;
    };

    // Shrink the widest column repeatedly until it fits (or all hit the floor).
    while (total_width() > max_width) {
        auto it = std::max_element(widths.begin(), widths.end());
        if (*it <= detail::kMinCol) break;
        --*it;
    }

    // Truncate any cell that overflows its (possibly shrunk) column.
    auto build_row = [&](const std::vector<std::string>& row) {
        std::string out = style.v;
        for (std::size_t c = 0; c < cols; ++c) {
            std::string cell = c < row.size() ? row[c] : std::string{};
            cell = truncate_ansi(cell, widths[c]);
            out += " " + detail::pad(cell, widths[c], al[c]) + " " + style.v;
        }
        return out;
    };

    std::string out;
    out += detail::rule(style, style.tl, style.tj, style.tr, widths) + "\n";
    out += build_row(cells[0]) + "\n";
    out += detail::rule(style, style.ml, style.mj, style.mr, widths) + "\n";
    for (std::size_t r = 1; r < cells.size(); ++r)
        out += build_row(cells[r]) + "\n";
    out += detail::rule(style, style.bl, style.bj, style.br, widths);
    return out;
}

}  // namespace md
