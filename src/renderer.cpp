// renderer.cpp — AST → ANSI terminal output.
//
// Inline content is flattened into styled "runs", then word-wrapped while
// tracking *visible* width so escape codes never count against the column
// budget. Block renderers compose child output and prefix each line (list
// markers, blockquote bars, heading hashes).
#include "renderer.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "ansi.hpp"
#include "table.hpp"
#include "text_util.hpp"
#include "unicode.hpp"

namespace md {
namespace {

// A maximal styled span of literal text plus an optional hyperlink target.
struct Run {
    std::string text;
    Style style;
    std::string url;
};

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& lines, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out += sep;
        out += lines[i];
    }
    return out;
}

std::string repeat(std::string_view glyph, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += glyph;
    return out;
}

Color heading_color(int level) {
    switch (level) {
        case 1: return Color::BrightMagenta;
        case 2: return Color::BrightBlue;
        case 3: return Color::BrightCyan;
        case 4: return Color::BrightGreen;
        case 5: return Color::BrightYellow;
        default: return Color::White;
    }
}

// Render one visible segment with its style and (optional) OSC 8 link.
std::string render_piece(const std::string& text, const Style& style,
                         const std::string& url) {
    std::string r = styled(text, style);
    if (g_color_enabled && !url.empty()) r = osc8(url, r);
    return r;
}

// Flatten inline nodes into styled runs, inheriting `base` style and `url`.
void flatten(const std::vector<InlineNode>& nodes, Style base,
             const std::string& url, std::vector<Run>& out) {
    for (const auto& n : nodes) {
        switch (n.kind) {
            case InlineKind::Text:
                out.push_back({n.text, base, url});
                break;
            case InlineKind::Code: {
                Style s = base;
                s.bold = false;
                s.italic = false;
                s.fg = Color::BrightYellow;
                out.push_back({n.text, s, url});
                break;
            }
            case InlineKind::Emphasis: {
                Style s = base;
                s.italic = true;
                flatten(n.children, s, url, out);
                break;
            }
            case InlineKind::Strong: {
                Style s = base;
                s.bold = true;
                flatten(n.children, s, url, out);
                break;
            }
            case InlineKind::StrongEmphasis: {
                Style s = base;
                s.bold = true;
                s.italic = true;
                flatten(n.children, s, url, out);
                break;
            }
            case InlineKind::Strikethrough: {
                Style s = base;
                s.strike = true;
                flatten(n.children, s, url, out);
                break;
            }
            case InlineKind::Link: {
                Style s = base;
                s.underline = true;
                s.fg = Color::BrightBlue;
                flatten(n.children, s, n.url, out);
                // Without a terminal to make the text clickable, show the URL.
                if (!g_color_enabled && !n.url.empty())
                    out.push_back({" (" + n.url + ")", base, ""});
                break;
            }
            case InlineKind::Image: {
                Style s = base;
                s.fg = Color::BrightMagenta;
                out.push_back({"🖼 ", s, n.url});
                flatten(n.children, s, n.url, out);
                if (!g_color_enabled && !n.url.empty())
                    out.push_back({" (" + n.url + ")", base, ""});
                break;
            }
        }
    }
}

// Word-wrap styled runs to `avail` visible columns.
std::vector<std::string> wrap(const std::vector<Run>& runs, int avail) {
    if (avail < 1) avail = 1;
    struct Word {
        std::string rendered;
        int width = 0;
    };
    std::vector<Word> words;
    Word cur;
    bool has = false;
    auto flush = [&] {
        if (has) {
            words.push_back(cur);
            cur = Word{};
            has = false;
        }
    };
    for (const auto& run : runs) {
        const std::string& t = run.text;
        std::size_t i = 0;
        while (i < t.size()) {
            if (t[i] == ' ' || t[i] == '\n' || t[i] == '\t') {
                flush();
                ++i;
                continue;
            }
            std::size_t j = i;
            while (j < t.size() &&
                   !(t[j] == ' ' || t[j] == '\n' || t[j] == '\t'))
                ++j;
            std::string seg = t.substr(i, j - i);
            cur.rendered += render_piece(seg, run.style, run.url);
            cur.width += display_width(seg);
            has = true;
            i = j;
        }
    }
    flush();

    std::vector<std::string> lines;
    std::string line;
    int lw = 0;
    for (const auto& w : words) {
        if (!line.empty() && lw + 1 + w.width > avail) {
            lines.push_back(line);
            line.clear();
            lw = 0;
        }
        if (line.empty()) {
            line = w.rendered;
            lw = w.width;
        } else {
            line += " " + w.rendered;
            lw += 1 + w.width;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// Render inline nodes to a single unwrapped line (for table cells).
std::string render_inline_flat(const std::vector<InlineNode>& nodes, Style base) {
    std::vector<Run> runs;
    flatten(nodes, base, "", runs);
    std::string out;
    for (const auto& r : runs) out += render_piece(r.text, r.style, r.url);
    return out;
}

// Prefix the first line with `first` and every subsequent line with `rest`.
std::string prefix_lines(const std::string& text, const std::string& first,
                         const std::string& rest) {
    std::vector<std::string> lines = split(text, '\n');
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out += '\n';
        out += (i == 0 ? first : rest) + lines[i];
    }
    return out;
}

int digits(int n) {
    int d = 1;
    for (n = n < 0 ? -n : n; n >= 10; n /= 10) ++d;
    return d;
}

std::string render_block(const BlockNode& b, int width);

std::string render_blocks(const std::vector<BlockNode>& blocks, int width,
                          const std::string& sep) {
    std::string out;
    bool first = true;
    for (const auto& b : blocks) {
        std::string s = render_block(b, width);
        if (!first) out += sep;
        out += s;
        first = false;
    }
    return out;
}

std::string render_heading(const BlockNode& b, int width) {
    std::string hashes = std::string(b.level, '#') + " ";
    std::string prefix = styled(hashes, Style{.dim = true});
    int pw = b.level + 1;

    Style hs;
    hs.bold = true;
    hs.fg = heading_color(b.level);
    std::vector<Run> runs;
    flatten(b.inlines, hs, "", runs);
    std::vector<std::string> lines = wrap(runs, std::max(1, width - pw));
    if (lines.empty()) lines.push_back("");

    std::string body =
        prefix_lines(join(lines, "\n"), prefix, std::string(pw, ' '));

    if (b.level <= 2) {
        int w = 0;
        for (const auto& l : lines) w = std::max(w, visible_width(l));
        int rule_len = std::min(width, pw + w);
        body += "\n" + styled(repeat("─", rule_len), Style{.dim = true});
    }
    return body;
}

std::string render_code_block(const BlockNode& b) {
    Style gutter{.fg = Color::BrightBlack};
    std::string bar = styled("│ ", gutter);
    std::string out;
    if (!b.info.empty())
        out += bar + styled(b.info, Style{.italic = true, .dim = true}) + "\n";
    std::vector<std::string> lines = split(b.literal, '\n');
    // split() of a trailing-newline-free literal is fine; drop a lone trailing
    // empty line that a final newline would produce.
    if (lines.size() > 1 && lines.back().empty()) lines.pop_back();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out += "\n";
        out += bar + lines[i];
    }
    return out;
}

std::string render_list(const BlockNode& b, int width) {
    int n = static_cast<int>(b.children.size());
    int num_w = b.ordered ? digits(b.start + std::max(0, n - 1)) : 1;
    std::string out;
    for (int idx = 0; idx < n; ++idx) {
        const BlockNode& item = b.children[idx];
        std::string marker;
        Style ms;
        if (item.task_state >= 0) {
            marker = item.task_state ? "☑ " : "☐ ";
            ms.fg = item.task_state ? Color::BrightGreen : Color::None;
        } else if (b.ordered) {
            std::string num = std::to_string(b.start + idx);
            num = std::string(num_w - static_cast<int>(num.size()), ' ') + num;
            marker = num + ". ";
            ms.fg = Color::BrightCyan;
        } else {
            marker = "• ";
            ms.fg = Color::BrightCyan;
        }
        int mw = display_width(marker);
        std::string content =
            render_blocks(item.children, std::max(1, width - mw), "\n");
        out += prefix_lines(content, styled(marker, ms), std::string(mw, ' '));
        if (idx + 1 < n) out += "\n";
    }
    return out;
}

std::string render_table_block(const BlockNode& b, int width) {
    std::vector<std::vector<std::string>> cells;
    for (std::size_t r = 0; r < b.rows.size(); ++r) {
        Style base;
        if (r == 0) base.bold = true;
        std::vector<std::string> row;
        for (const auto& cell : b.rows[r])
            row.push_back(render_inline_flat(cell, base));
        cells.push_back(std::move(row));
    }
    return render_table(cells, b.aligns, width);
}

std::string render_block(const BlockNode& b, int width) {
    switch (b.kind) {
        case BlockKind::Document:
            return render_blocks(b.children, width, "\n\n");
        case BlockKind::Paragraph: {
            std::vector<Run> runs;
            flatten(b.inlines, Style{}, "", runs);
            return join(wrap(runs, width), "\n");
        }
        case BlockKind::Heading:
            return render_heading(b, width);
        case BlockKind::CodeBlock:
            return render_code_block(b);
        case BlockKind::BlockQuote: {
            std::string inner =
                render_blocks(b.children, std::max(1, width - 2), "\n\n");
            std::string bar = styled("▌ ", Style{.fg = Color::BrightBlack});
            return prefix_lines(inner, bar, bar);
        }
        case BlockKind::List:
            return render_list(b, width);
        case BlockKind::ThematicBreak:
            return styled(repeat("─", width), Style{.dim = true});
        case BlockKind::Table:
            return render_table_block(b, width);
        case BlockKind::ListItem:
            return render_blocks(b.children, width, "\n");
    }
    return {};
}

}  // namespace

std::string render_document(const std::vector<BlockNode>& blocks, int width) {
    return render_blocks(blocks, width, "\n\n");
}

}  // namespace md
