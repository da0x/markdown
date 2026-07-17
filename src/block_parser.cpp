// block_parser.cpp — a line-oriented block scanner.
//
// The source is split into lines once, then parse() walks a [begin, end) range
// dispatching on each line's shape. Container blocks (blockquotes, list items)
// strip their marker and recurse on the inner lines, which keeps nesting simple.
#include "block_parser.hpp"

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>

#include "inline_parser.hpp"

namespace md {
namespace {

constexpr std::size_t npos = std::string_view::npos;

std::string rstrip(std::string_view s) {
    std::size_t e = s.size();
    while (e > 0 && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
        --e;
    return std::string(s.substr(0, e));
}

std::string_view lstrip_view(std::string_view s) {
    std::size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
    return s.substr(b);
}

bool is_blank(std::string_view s) {
    return lstrip_view(s).empty();
}

int leading_spaces(std::string_view s) {
    int n = 0;
    for (char c : s) {
        if (c == ' ') ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

std::vector<std::string> split_lines(std::string_view src) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= src.size(); ++i) {
        if (i == src.size() || src[i] == '\n') {
            lines.emplace_back(rstrip(src.substr(start, i - start)));
            start = i + 1;
        }
    }
    // A trailing newline yields one empty final element; drop it.
    if (!lines.empty() && lines.back().empty() && !src.empty() &&
        src.back() == '\n')
        lines.pop_back();
    return lines;
}

// ---- line classifiers ---------------------------------------------------

struct HeadingInfo {
    int level;
    std::string text;
};

std::optional<HeadingInfo> match_heading(std::string_view line) {
    std::string_view s = lstrip_view(line);
    if (leading_spaces(line) > 3) return std::nullopt;
    int level = 0;
    while (level < static_cast<int>(s.size()) && s[level] == '#') ++level;
    if (level == 0 || level > 6) return std::nullopt;
    if (level < static_cast<int>(s.size()) && s[level] != ' ') return std::nullopt;
    std::string_view rest = lstrip_view(s.substr(level));
    // Strip an optional closing run of '#'.
    std::string text = rstrip(rest);
    std::size_t e = text.size();
    while (e > 0 && text[e - 1] == '#') --e;
    if (e < text.size() && (e == 0 || text[e - 1] == ' '))
        text = rstrip(text.substr(0, e));
    return HeadingInfo{level, text};
}

struct FenceInfo {
    char ch;
    int len;
    int indent;
    std::string info;
};

std::optional<FenceInfo> match_fence(std::string_view line) {
    if (leading_spaces(line) > 3) return std::nullopt;
    std::string_view s = lstrip_view(line);
    if (s.empty() || (s[0] != '`' && s[0] != '~')) return std::nullopt;
    char ch = s[0];
    int len = 0;
    while (len < static_cast<int>(s.size()) && s[len] == ch) ++len;
    if (len < 3) return std::nullopt;
    std::string info = rstrip(lstrip_view(s.substr(len)));
    // Info strings for backtick fences may not contain a backtick.
    if (ch == '`' && info.find('`') != std::string::npos) return std::nullopt;
    return FenceInfo{ch, len, leading_spaces(line), info};
}

bool match_thematic_break(std::string_view line) {
    if (leading_spaces(line) > 3) return false;
    std::string_view s = lstrip_view(line);
    if (s.empty()) return false;
    char c = s[0];
    if (c != '-' && c != '*' && c != '_') return false;
    int count = 0;
    for (char ch : s) {
        if (ch == c) ++count;
        else if (ch != ' ' && ch != '\t') return false;
    }
    return count >= 3;
}

bool is_blockquote(std::string_view line) {
    if (leading_spaces(line) > 3) return false;
    std::string_view s = lstrip_view(line);
    return !s.empty() && s[0] == '>';
}

// Strip one level of "> " from a blockquote line.
std::string strip_blockquote(std::string_view line) {
    std::string_view s = lstrip_view(line);
    s.remove_prefix(1);  // '>'
    if (!s.empty() && s[0] == ' ') s.remove_prefix(1);
    return std::string(s);
}

struct ListMarker {
    int indent;         // leading spaces before the marker
    int content_indent; // column where the item's content begins
    bool ordered;
    int start;
    char bullet;        // '-', '*', '+' for bullets
};

std::optional<ListMarker> match_list(std::string_view line) {
    int indent = leading_spaces(line);
    if (indent > 3 && lstrip_view(line).empty()) return std::nullopt;
    std::string_view s = lstrip_view(line);
    if (s.empty()) return std::nullopt;

    if (s[0] == '-' || s[0] == '*' || s[0] == '+') {
        if (s.size() < 2 || s[1] != ' ') return std::nullopt;
        return ListMarker{indent, indent + 2, false, 1, s[0]};
    }
    // Ordered: digits then '.' or ')'.
    int d = 0;
    while (d < static_cast<int>(s.size()) && std::isdigit((unsigned char)s[d]))
        ++d;
    if (d == 0 || d > 9) return std::nullopt;
    if (d >= static_cast<int>(s.size()) || (s[d] != '.' && s[d] != ')'))
        return std::nullopt;
    if (d + 1 >= static_cast<int>(s.size()) || s[d + 1] != ' ')
        return std::nullopt;
    int start = std::stoi(std::string(s.substr(0, d)));
    return ListMarker{indent, indent + d + 2, true, start, 0};
}

// A GFM delimiter row: pipes, dashes, colons, spaces, at least one dash.
bool is_table_delimiter(std::string_view line) {
    std::string_view s = lstrip_view(line);
    if (s.empty()) return false;
    bool dash = false;
    for (char c : s) {
        if (c == '-') dash = true;
        else if (c != '|' && c != ':' && c != ' ' && c != '\t') return false;
    }
    return dash && s.find('-') != npos;
}

// Split a table row on unescaped pipes, trimming cells and the optional
// leading/trailing empty cell from surrounding pipes.
std::vector<std::string> split_row(std::string_view line) {
    std::string s = rstrip(lstrip_view(line));
    std::vector<std::string> cells;
    std::string cur;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '|') {
            cur += '|';
            ++i;
        } else if (s[i] == '|') {
            cells.push_back(cur);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    cells.push_back(cur);
    if (!cells.empty() && cells.front().find_first_not_of(" \t") == npos)
        cells.erase(cells.begin());
    if (!cells.empty() && cells.back().find_first_not_of(" \t") == npos)
        cells.pop_back();
    for (auto& c : cells) c = rstrip(lstrip_view(c));
    return cells;
}

// ---- the parser --------------------------------------------------------

class BlockParser {
public:
    explicit BlockParser(const std::vector<std::string>& lines) : L(lines) {}

    std::vector<BlockNode> parse(std::size_t begin, std::size_t end) {
        std::vector<BlockNode> out;
        std::size_t i = begin;
        while (i < end) {
            if (is_blank(L[i])) {
                ++i;
                continue;
            }
            if (auto f = match_fence(L[i])) {
                out.push_back(parse_code(i, end, *f));
            } else if (auto h = match_heading(L[i])) {
                BlockNode n{.kind = BlockKind::Heading, .level = h->level};
                n.inlines = parse_inlines(h->text);
                out.push_back(std::move(n));
                ++i;
            } else if (match_thematic_break(L[i])) {
                out.push_back({.kind = BlockKind::ThematicBreak});
                ++i;
            } else if (is_blockquote(L[i])) {
                out.push_back(parse_blockquote(i, end));
            } else if (match_list(L[i])) {
                out.push_back(parse_list(i, end));
            } else if (i + 1 < end && L[i].find('|') != npos &&
                       is_table_delimiter(L[i + 1])) {
                out.push_back(parse_table(i, end));
            } else {
                out.push_back(parse_paragraph(i, end));
            }
        }
        return out;
    }

private:
    const std::vector<std::string>& L;

    // Does this line begin a block other than a paragraph continuation?
    bool starts_block(std::size_t i, std::size_t end) {
        const std::string& line = L[i];
        if (is_blank(line)) return true;
        if (match_fence(line) || match_heading(line) ||
            match_thematic_break(line) || is_blockquote(line) ||
            match_list(line))
            return true;
        if (i + 1 < end && line.find('|') != npos &&
            is_table_delimiter(L[i + 1]))
            return true;
        return false;
    }

    BlockNode parse_code(std::size_t& i, std::size_t end, const FenceInfo& f) {
        BlockNode n{.kind = BlockKind::CodeBlock, .info = f.info};
        ++i;  // past opening fence
        std::string body;
        bool first = true;
        while (i < end) {
            std::string_view s = lstrip_view(L[i]);
            bool closing = false;
            if (!s.empty() && s[0] == f.ch) {
                int len = 0;
                while (len < (int)s.size() && s[len] == f.ch) ++len;
                if (len >= f.len && rstrip(s.substr(len)).empty()) closing = true;
            }
            if (closing) {
                ++i;
                break;
            }
            // Strip up to the fence's own indentation from each content line.
            std::string_view line = L[i];
            int strip = 0;
            while (strip < f.indent && strip < (int)line.size() &&
                   line[strip] == ' ')
                ++strip;
            if (!first) body += '\n';
            body += line.substr(strip);
            first = false;
            ++i;
        }
        n.literal = body;
        return n;
    }

    BlockNode parse_blockquote(std::size_t& i, std::size_t end) {
        std::vector<std::string> inner;
        while (i < end && is_blockquote(L[i])) {
            inner.push_back(strip_blockquote(L[i]));
            ++i;
        }
        BlockParser sub(inner);
        BlockNode n{.kind = BlockKind::BlockQuote};
        n.children = sub.parse(0, inner.size());
        return n;
    }

    BlockNode parse_list(std::size_t& i, std::size_t end) {
        ListMarker first = *match_list(L[i]);
        BlockNode list{.kind = BlockKind::List,
                       .ordered = first.ordered,
                       .start = first.start};

        while (i < end) {
            auto m = match_list(L[i]);
            if (!m) break;
            // A different bullet character or ordered/unordered switch starts a
            // new list, not another item of this one.
            if (m->ordered != first.ordered ||
                (!m->ordered && m->bullet != first.bullet) ||
                m->indent != first.indent)
                break;

            BlockNode item = parse_item(i, end, *m);
            list.children.push_back(std::move(item));

            // Skip a single blank separator between items (loose list).
            std::size_t save = i;
            while (i < end && is_blank(L[i])) ++i;
            if (i < end) {
                auto nm = match_list(L[i]);
                if (!nm || nm->indent != first.indent ||
                    nm->ordered != first.ordered ||
                    (!nm->ordered && nm->bullet != first.bullet)) {
                    i = save;  // not a continuation of this list
                    break;
                }
            } else {
                break;
            }
        }
        return list;
    }

    BlockNode parse_item(std::size_t& i, std::size_t end, const ListMarker& m) {
        // First line: text after the marker.
        std::vector<std::string> inner;
        std::string_view first_line = L[i];
        inner.emplace_back(first_line.substr(
            std::min<std::size_t>(m.content_indent, first_line.size())));
        ++i;

        // Continuation lines: blanks, or lines indented into the content column.
        while (i < end) {
            if (is_blank(L[i])) {
                // Peek: does the item continue after the blank line?
                std::size_t j = i + 1;
                while (j < end && is_blank(L[j])) ++j;
                if (j < end && leading_spaces(L[j]) >= m.content_indent) {
                    inner.emplace_back("");
                    i = j;
                    continue;
                }
                break;
            }
            if (leading_spaces(L[i]) >= m.content_indent) {
                inner.emplace_back(L[i].substr(m.content_indent));
                ++i;
            } else if (!starts_block(i, end)) {
                // Lazy paragraph continuation.
                inner.emplace_back(L[i]);
                ++i;
            } else {
                break;
            }
        }

        BlockParser sub(inner);
        BlockNode item{.kind = BlockKind::ListItem};
        item.children = sub.parse(0, inner.size());
        detect_task(item);
        return item;
    }

    // Promote a leading "[ ] "/"[x] " on the item's first paragraph to a task
    // checkbox state.
    static void detect_task(BlockNode& item) {
        if (item.children.empty() ||
            item.children.front().kind != BlockKind::Paragraph)
            return;
        auto& para = item.children.front();
        if (para.inlines.empty() ||
            para.inlines.front().kind != InlineKind::Text)
            return;
        std::string& t = para.inlines.front().text;
        if (t.size() >= 4 && t[0] == '[' && t[2] == ']' && t[3] == ' ' &&
            (t[1] == ' ' || t[1] == 'x' || t[1] == 'X')) {
            item.task_state = (t[1] == ' ') ? 0 : 1;
            t.erase(0, 4);
        }
    }

    BlockNode parse_table(std::size_t& i, std::size_t end) {
        BlockNode n{.kind = BlockKind::Table};
        std::vector<std::string> header = split_row(L[i]);
        std::vector<std::string> delim = split_row(L[i + 1]);
        std::size_t cols = header.size();

        n.aligns.resize(cols, Align::Left);
        for (std::size_t c = 0; c < cols && c < delim.size(); ++c) {
            const std::string& d = delim[c];
            bool left = !d.empty() && d.front() == ':';
            bool right = !d.empty() && d.back() == ':';
            if (left && right) n.aligns[c] = Align::Center;
            else if (right) n.aligns[c] = Align::Right;
            else n.aligns[c] = Align::Left;
        }

        auto add_row = [&](std::vector<std::string> cells) {
            cells.resize(cols);  // pad/truncate to the header's column count
            std::vector<std::vector<InlineNode>> row;
            for (auto& cell : cells) row.push_back(parse_inlines(cell));
            n.rows.push_back(std::move(row));
        };

        add_row(header);
        i += 2;
        while (i < end && !is_blank(L[i]) && L[i].find('|') != npos) {
            add_row(split_row(L[i]));
            ++i;
        }
        return n;
    }

    BlockNode parse_paragraph(std::size_t& i, std::size_t end) {
        std::string text;
        bool first = true;
        while (i < end && !starts_block(i, end)) {
            if (!first) text += '\n';
            text += rstrip(lstrip_view(L[i]));
            first = false;
            ++i;
        }
        BlockNode n{.kind = BlockKind::Paragraph};
        n.inlines = parse_inlines(text);
        return n;
    }
};

}  // namespace

std::vector<BlockNode> parse_document(std::string_view source) {
    std::vector<std::string> lines = split_lines(source);
    BlockParser parser(lines);
    return parser.parse(0, lines.size());
}

}  // namespace md
