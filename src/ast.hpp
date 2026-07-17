// ast.hpp — the document tree produced by the parsers and consumed by the
// renderer.
#pragma once

#include <string>
#include <vector>

namespace md {

// ---- Inline nodes -------------------------------------------------------
// Inline content is a flat-ish tree: styled spans may nest (e.g. bold inside a
// link). `Text` and `Code` are leaves carrying literal text; the styling kinds
// carry `children`; `Link`/`Image` additionally carry `url`.
enum class InlineKind {
    Text,
    Code,
    Emphasis,        // *italic*
    Strong,          // **bold**
    StrongEmphasis,  // ***bold italic***
    Strikethrough,   // ~~strike~~
    Link,            // [text](url)
    Image,           // ![alt](url)
};

struct InlineNode {
    InlineKind kind;
    std::string text;                 // literal, for Text/Code
    std::string url;                  // for Link/Image
    std::vector<InlineNode> children; // for styled spans, Link, Image
};

// ---- Block nodes --------------------------------------------------------
enum class BlockKind {
    Document,
    Heading,
    Paragraph,
    CodeBlock,
    BlockQuote,
    List,
    ListItem,
    ThematicBreak,
    Table,
};

enum class Align { Left, Center, Right };

struct BlockNode {
    BlockKind kind;

    // Heading
    int level = 0;

    // List / ListItem
    bool ordered = false;   // list: ordered vs bullet
    int start = 1;          // list: first ordinal
    int task_state = -1;    // list item: -1 none, 0 unchecked, 1 checked

    // CodeBlock
    std::string info;       // language / info string
    std::string literal;    // raw code, newline-separated

    // Table
    std::vector<Align> aligns;                            // per column
    std::vector<std::vector<std::vector<InlineNode>>> rows;  // row -> cell -> inlines
    // rows[0] is the header row.

    // Heading / Paragraph inline content
    std::vector<InlineNode> inlines;

    // Nested blocks (Document, BlockQuote, List, ListItem)
    std::vector<BlockNode> children;
};

}  // namespace md
