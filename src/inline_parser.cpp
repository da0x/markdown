// inline_parser.cpp — a pragmatic inline Markdown scanner.
//
// This is not a full CommonMark delimiter-run implementation; it handles the
// well-formed constructs that appear in real documents (matched emphasis, code
// spans, links, images, autolinks, escapes) and renders anything ambiguous as
// literal text.
#include "inline_parser.hpp"

#include <cctype>
#include <cstddef>
#include <string>

namespace md {
namespace {

constexpr std::size_t npos = std::string_view::npos;

bool is_ascii_punct(char c) {
    static const std::string_view p = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    return p.find(c) != npos;
}

bool is_alnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

// Length of the run of character `c` starting at `i`.
std::size_t run_len(std::string_view s, std::size_t i, char c) {
    std::size_t j = i;
    while (j < s.size() && s[j] == c) ++j;
    return j - i;
}

struct Scanner {
    std::string_view s;
    std::string buf;                 // pending literal text
    std::vector<InlineNode> out;

    void push_text() {
        if (!buf.empty()) {
            out.push_back({InlineKind::Text, buf, {}, {}});
            buf.clear();
        }
    }

    void emit(InlineNode n) {
        push_text();
        out.push_back(std::move(n));
    }

    // Find a closing delimiter run of `c` at least `len` long, at or after
    // `from`. Runs shorter than `len` are skipped over (treated as literal).
    std::size_t find_closer(std::size_t from, char c, std::size_t len) {
        std::size_t j = from;
        while (j < s.size()) {
            if (s[j] == c) {
                std::size_t rl = run_len(s, j, c);
                if (rl >= len) return j;
                j += rl;
            } else {
                ++j;
            }
        }
        return npos;
    }

    // `_` only delimits at a word boundary so intra-word underscores in
    // identifiers (foo_bar_baz) stay literal. `*` may delimit intra-word.
    bool underscore_ok_open(std::size_t i) {
        return i == 0 || !is_alnum(s[i - 1]);
    }
    bool underscore_ok_close(std::size_t end) {
        return end >= s.size() || !is_alnum(s[end]);
    }

    void run() {
        std::size_t i = 0;
        while (i < s.size()) {
            char c = s[i];

            // Backslash escape.
            if (c == '\\' && i + 1 < s.size() && is_ascii_punct(s[i + 1])) {
                buf += s[i + 1];
                i += 2;
                continue;
            }

            // Code span: `code`, ``code with ` inside``.
            if (c == '`') {
                std::size_t len = run_len(s, i, '`');
                std::size_t close = s.find(std::string(len, '`'), i + len);
                // A closer must be exactly `len` backticks (not part of a longer
                // run); scan for one.
                while (close != npos && run_len(s, close, '`') != len)
                    close = s.find(std::string(len, '`'), close + 1);
                if (close != npos) {
                    std::string code(s.substr(i + len, close - (i + len)));
                    // CommonMark: strip one surrounding space if both present.
                    if (code.size() >= 2 && code.front() == ' ' &&
                        code.back() == ' ' &&
                        code.find_first_not_of(' ') != npos) {
                        code = code.substr(1, code.size() - 2);
                    }
                    emit({InlineKind::Code, code, {}, {}});
                    i = close + len;
                    continue;
                }
            }

            // Image: ![alt](url)
            if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {
                std::size_t r = parse_link(i + 1, true);
                if (r != npos) {
                    i = r;
                    continue;
                }
            }

            // Link: [text](url)
            if (c == '[') {
                std::size_t r = parse_link(i, false);
                if (r != npos) {
                    i = r;
                    continue;
                }
            }

            // Autolink: <https://…> or <name@host>
            if (c == '<') {
                std::size_t r = parse_autolink(i);
                if (r != npos) {
                    i = r;
                    continue;
                }
            }

            // Emphasis / strong / strikethrough.
            if (c == '*' || c == '_' || c == '~') {
                std::size_t r = parse_emphasis(i, c);
                if (r != npos) {
                    i = r;
                    continue;
                }
            }

            buf += c;
            ++i;
        }
        push_text();
    }

    // Returns index past the construct, or npos if it isn't a valid link.
    std::size_t parse_link(std::size_t open_bracket, bool image) {
        // Find the matching ']' accounting for nested brackets.
        int depth = 0;
        std::size_t j = open_bracket;
        std::size_t close_bracket = npos;
        for (; j < s.size(); ++j) {
            if (s[j] == '\\') {
                ++j;
                continue;
            }
            if (s[j] == '[') ++depth;
            else if (s[j] == ']') {
                if (--depth == 0) {
                    close_bracket = j;
                    break;
                }
            }
        }
        if (close_bracket == npos) return npos;
        std::size_t paren = close_bracket + 1;
        if (paren >= s.size() || s[paren] != '(') return npos;

        // URL runs until the closing paren; an optional "title" is dropped.
        std::size_t k = paren + 1;
        std::string url;
        while (k < s.size() && s[k] != ')') {
            if (s[k] == ' ') break;  // start of a title
            url += s[k];
            ++k;
        }
        std::size_t end_paren = s.find(')', k);
        if (end_paren == npos) return npos;

        std::string_view inner =
            s.substr(open_bracket + 1, close_bracket - open_bracket - 1);
        InlineNode node;
        node.kind = image ? InlineKind::Image : InlineKind::Link;
        node.url = url;
        node.children = parse_inlines(inner);
        emit(std::move(node));
        return end_paren + 1;
    }

    std::size_t parse_autolink(std::size_t lt) {
        std::size_t gt = s.find('>', lt + 1);
        if (gt == npos) return npos;
        std::string_view body = s.substr(lt + 1, gt - lt - 1);
        if (body.empty() || body.find(' ') != npos) return npos;
        bool url = body.starts_with("http://") || body.starts_with("https://");
        bool email = !url && body.find('@') != npos && body.find('.') != npos;
        if (!url && !email) return npos;
        InlineNode node;
        node.kind = InlineKind::Link;
        node.url = email ? "mailto:" + std::string(body) : std::string(body);
        node.children = {{InlineKind::Text, std::string(body), {}, {}}};
        emit(std::move(node));
        return gt + 1;
    }

    std::size_t parse_emphasis(std::size_t i, char c) {
        std::size_t rl = run_len(s, i, c);
        if (c == '~') {
            if (rl < 2) return npos;  // only ~~strike~~
            rl = 2;
        } else {
            if (rl > 3) rl = 3;
        }
        if (c == '_' && !underscore_ok_open(i)) return npos;

        std::size_t content = i + rl;
        std::size_t close = find_closer(content, c, rl);
        if (close == npos) return npos;
        if (close == content) return npos;  // empty span
        if (c == '_' && !underscore_ok_close(close + rl)) return npos;

        std::string_view inner = s.substr(content, close - content);
        InlineNode node;
        if (c == '~') node.kind = InlineKind::Strikethrough;
        else if (rl == 3) node.kind = InlineKind::StrongEmphasis;
        else if (rl == 2) node.kind = InlineKind::Strong;
        else node.kind = InlineKind::Emphasis;
        node.children = parse_inlines(inner);
        emit(std::move(node));
        return close + rl;
    }
};

}  // namespace

std::vector<InlineNode> parse_inlines(std::string_view text) {
    Scanner sc{text, {}, {}};
    sc.run();
    return std::move(sc.out);
}

}  // namespace md
