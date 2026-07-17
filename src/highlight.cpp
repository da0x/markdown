// highlight.cpp — the syntax-highlighting lexer and language registry.
#include "highlight.hpp"

#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ansi.hpp"

namespace md {
namespace {

constexpr std::size_t npos = std::string::npos;

// ---- token palette ------------------------------------------------------
const Style kComment{.italic = true, .fg = Color::BrightBlack};
const Style kString{.fg = Color::BrightGreen};
const Style kNumber{.fg = Color::BrightCyan};
const Style kKeyword{.bold = true, .fg = Color::BrightMagenta};
const Style kType{.fg = Color::BrightYellow};
const Style kFunc{.fg = Color::BrightBlue};

// ---- language description ----------------------------------------------
struct LangSpec {
    std::vector<std::string> line_comments;         // e.g. {"//", "#"}
    std::string block_open, block_close;            // "" when unsupported
    std::vector<std::string> multiline_strings;     // e.g. {"\"\"\"", "'''"}
    std::string strings;                            // single-char delimiters, e.g. "\"'`"
    std::unordered_set<std::string> keywords;
    std::unordered_set<std::string> types;          // builtins / type names
    bool functions = false;                         // color name-before-'(' calls
};

using SV = std::vector<std::string>;
using WS = std::unordered_set<std::string>;

const LangSpec& lang_default() {
    static const LangSpec s{{}, "", "", {}, "\"'", {}, {}, false};
    return s;
}

const LangSpec& lang_cpp() {
    static const LangSpec s{
        {"//"}, "/*", "*/", {}, "\"'",
        WS{"alignas","alignof","and","asm","auto","bool","break","case","catch",
           "char","char8_t","char16_t","char32_t","class","concept","const",
           "consteval","constexpr","constinit","const_cast","continue","co_await",
           "co_return","co_yield","decltype","default","delete","do","double",
           "dynamic_cast","else","enum","explicit","export","extern","false",
           "float","for","friend","goto","if","inline","int","long","mutable",
           "namespace","new","noexcept","nullptr","operator","private","protected",
           "public","register","reinterpret_cast","requires","return","short",
           "signed","sizeof","static","static_assert","static_cast","struct",
           "switch","template","this","thread_local","throw","true","try",
           "typedef","typeid","typename","union","unsigned","using","virtual",
           "void","volatile","wchar_t","while"},
        WS{"size_t","ssize_t","ptrdiff_t","int8_t","int16_t","int32_t","int64_t",
           "uint8_t","uint16_t","uint32_t","uint64_t","string","string_view",
           "wstring","vector","map","unordered_map","set","unordered_set","array",
           "pair","tuple","optional","variant","span","std"},
        true};
    return s;
}

const LangSpec& lang_c() {
    static const LangSpec s{
        {"//"}, "/*", "*/", {}, "\"'",
        WS{"auto","break","case","char","const","continue","default","do",
           "double","else","enum","extern","float","for","goto","if","inline",
           "int","long","register","restrict","return","short","signed","sizeof",
           "static","struct","switch","typedef","union","unsigned","void",
           "volatile","while","_Bool","bool","true","false","NULL"},
        WS{"size_t","int8_t","int16_t","int32_t","int64_t","uint8_t","uint16_t",
           "uint32_t","uint64_t","FILE"},
        true};
    return s;
}

const LangSpec& lang_python() {
    static const LangSpec s{
        {"#"}, "", "", SV{"\"\"\"", "'''"}, "\"'",
        WS{"False","None","True","and","as","assert","async","await","break",
           "class","continue","def","del","elif","else","except","finally","for",
           "from","global","if","import","in","is","lambda","nonlocal","not","or",
           "pass","raise","return","try","while","with","yield","match","case"},
        WS{"print","len","range","int","float","str","list","dict","tuple","set",
           "bytes","bool","object","super","isinstance","enumerate","zip","map",
           "filter","open","self","cls","abs","min","max","sum","sorted"},
        true};
    return s;
}

const LangSpec& lang_js() {
    static const LangSpec s{
        {"//"}, "/*", "*/", {}, "\"'`",
        WS{"async","await","break","case","catch","class","const","continue",
           "debugger","default","delete","do","else","export","extends","false",
           "finally","for","from","function","if","import","in","instanceof",
           "let","new","null","of","return","static","super","switch","this",
           "throw","true","try","typeof","undefined","var","void","while","yield",
           "interface","type","enum","implements","public","private","protected",
           "readonly","namespace","declare","abstract","as","is","keyof"},
        WS{"console","document","window","Math","JSON","Object","Array","String",
           "Number","Boolean","Promise","Map","Set","Symbol","number","string",
           "boolean","any","unknown","never","void"},
        true};
    return s;
}

const LangSpec& lang_rust() {
    // No single-quote strings: '\'' would swallow lifetimes like 'a.
    static const LangSpec s{
        {"//"}, "/*", "*/", {}, "\"",
        WS{"as","async","await","break","const","continue","crate","dyn","else",
           "enum","extern","false","fn","for","if","impl","in","let","loop",
           "match","mod","move","mut","pub","ref","return","self","Self","static",
           "struct","super","trait","true","type","unsafe","use","where","while"},
        WS{"i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128",
           "usize","f32","f64","bool","char","str","String","Vec","Option","Result",
           "Box","Some","None","Ok","Err","Rc","Arc","HashMap","HashSet"},
        true};
    return s;
}

const LangSpec& lang_go() {
    static const LangSpec s{
        {"//"}, "/*", "*/", {}, "\"'`",
        WS{"break","case","chan","const","continue","default","defer","else",
           "fallthrough","for","func","go","goto","if","import","interface","map",
           "package","range","return","select","struct","switch","type","var"},
        WS{"bool","byte","complex64","complex128","error","float32","float64",
           "int","int8","int16","int32","int64","rune","string","uint","uint8",
           "uint16","uint32","uint64","uintptr","true","false","nil","iota",
           "append","len","cap","make","new","panic","recover","print","println"},
        true};
    return s;
}

const LangSpec& lang_shell() {
    static const LangSpec s{
        {"#"}, "", "", {}, "\"'",
        WS{"if","then","else","elif","fi","case","esac","for","while","until",
           "do","done","in","function","select","time","return","break",
           "continue","local","export","readonly","declare"},
        WS{"echo","cd","pwd","ls","cat","grep","sed","awk","cp","mv","rm","mkdir",
           "make","git","sudo","source","read","set","unset","exit"},
        false};
    return s;
}

const LangSpec& lang_json() {
    static const LangSpec s{
        {}, "", "", {}, "\"",
        WS{"true","false","null"}, {}, false};
    return s;
}

const LangSpec& spec_for(std::string_view lang) {
    static const std::unordered_map<std::string, const LangSpec& (*)()> reg = {
        {"cpp", lang_cpp}, {"c++", lang_cpp}, {"cxx", lang_cpp},
        {"cc", lang_cpp}, {"hpp", lang_cpp}, {"hxx", lang_cpp}, {"h", lang_cpp},
        {"c", lang_c},
        {"py", lang_python}, {"python", lang_python},
        {"js", lang_js}, {"javascript", lang_js}, {"jsx", lang_js},
        {"ts", lang_js}, {"typescript", lang_js}, {"tsx", lang_js},
        {"rs", lang_rust}, {"rust", lang_rust},
        {"go", lang_go}, {"golang", lang_go},
        {"sh", lang_shell}, {"bash", lang_shell}, {"shell", lang_shell},
        {"zsh", lang_shell},
        {"json", lang_json},
    };
    auto it = reg.find(std::string(lang));
    return it == reg.end() ? lang_default() : it->second();
}

// ---- the lexer ----------------------------------------------------------
enum class Mode { Normal, Block, MultiStr };
struct State {
    Mode mode = Mode::Normal;
    std::string closer;  // block_close, or the multiline-string delimiter
};

bool ident_start(unsigned char c) {
    return std::isalpha(c) || c == '_' || c == '$' || c >= 0x80;
}
bool ident_part(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '$' || c >= 0x80;
}

bool matches_at(const std::string& s, std::size_t i, const std::string& tok) {
    return !tok.empty() && s.compare(i, tok.size(), tok) == 0;
}

std::string highlight_line(const std::string& line, const LangSpec& spec,
                           State& st) {
    std::string out;
    std::size_t i = 0, n = line.size();
    auto put = [&](std::size_t from, std::size_t to, const Style& style) {
        out += styled(line.substr(from, to - from), style);
    };

    // Continuation of a multi-line construct started on a previous line.
    if (st.mode != Mode::Normal) {
        const Style& style = st.mode == Mode::Block ? kComment : kString;
        std::size_t pos = line.find(st.closer);
        if (pos == npos) {
            out += styled(line, style);
            return out;
        }
        put(0, pos + st.closer.size(), style);
        i = pos + st.closer.size();
        st.mode = Mode::Normal;
    }

    while (i < n) {
        char c = line[i];

        // Line comment: rest of the line.
        bool did = false;
        for (const auto& lc : spec.line_comments) {
            if (matches_at(line, i, lc)) {
                out += styled(line.substr(i), kComment);
                i = n;
                did = true;
                break;
            }
        }
        if (did) break;

        // Block comment (may span lines).
        if (matches_at(line, i, spec.block_open)) {
            std::size_t pos = line.find(spec.block_close, i + spec.block_open.size());
            if (pos == npos) {
                out += styled(line.substr(i), kComment);
                st.mode = Mode::Block;
                st.closer = spec.block_close;
                i = n;
            } else {
                put(i, pos + spec.block_close.size(), kComment);
                i = pos + spec.block_close.size();
            }
            continue;
        }

        // Multi-line string (triple-quote and friends).
        did = false;
        for (const auto& ml : spec.multiline_strings) {
            if (matches_at(line, i, ml)) {
                std::size_t pos = line.find(ml, i + ml.size());
                if (pos == npos) {
                    out += styled(line.substr(i), kString);
                    st.mode = Mode::MultiStr;
                    st.closer = ml;
                    i = n;
                } else {
                    put(i, pos + ml.size(), kString);
                    i = pos + ml.size();
                }
                did = true;
                break;
            }
        }
        if (did) continue;

        // Single-line string with backslash escapes.
        if (spec.strings.find(c) != npos) {
            std::size_t j = i + 1;
            while (j < n) {
                if (line[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (line[j] == c) {
                    ++j;
                    break;
                }
                ++j;
            }
            put(i, j, kString);
            i = j;
            continue;
        }

        // Number literal.
        if (std::isdigit((unsigned char)c) ||
            (c == '.' && i + 1 < n && std::isdigit((unsigned char)line[i + 1]))) {
            std::size_t j = i;
            if (c == '0' && i + 1 < n &&
                (line[i + 1] == 'x' || line[i + 1] == 'X')) {
                j = i + 2;
                while (j < n && std::isxdigit((unsigned char)line[j])) ++j;
            } else {
                while (j < n && std::isdigit((unsigned char)line[j])) ++j;
                if (j < n && line[j] == '.') {
                    ++j;
                    while (j < n && std::isdigit((unsigned char)line[j])) ++j;
                }
                if (j < n && (line[j] == 'e' || line[j] == 'E')) {
                    ++j;
                    if (j < n && (line[j] == '+' || line[j] == '-')) ++j;
                    while (j < n && std::isdigit((unsigned char)line[j])) ++j;
                }
            }
            while (j < n && (std::isalpha((unsigned char)line[j]) ||
                             line[j] == '_'))
                ++j;  // suffixes: 10u, 1.5f, 3px
            put(i, j, kNumber);
            i = j;
            continue;
        }

        // Identifier / keyword / type / function call.
        if (ident_start((unsigned char)c)) {
            std::size_t j = i;
            while (j < n && ident_part((unsigned char)line[j])) ++j;
            std::string word = line.substr(i, j - i);
            if (spec.keywords.count(word)) {
                out += styled(word, kKeyword);
            } else if (spec.types.count(word)) {
                out += styled(word, kType);
            } else if (spec.functions) {
                std::size_t k = j;
                while (k < n && line[k] == ' ') ++k;
                if (k < n && line[k] == '(')
                    out += styled(word, kFunc);
                else
                    out += word;
            } else {
                out += word;
            }
            i = j;
            continue;
        }

        out += c;
        ++i;
    }
    return out;
}

}  // namespace

std::vector<std::string> highlight_code(const std::vector<std::string>& lines,
                                        std::string_view lang) {
    if (!g_color_enabled) return lines;
    const LangSpec& spec = spec_for(lang);
    State st;
    std::vector<std::string> out;
    out.reserve(lines.size());
    for (const auto& line : lines) out.push_back(highlight_line(line, spec, st));
    return out;
}

}  // namespace md
