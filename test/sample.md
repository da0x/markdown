# md — Terminal Markdown Renderer

A **greenfield** C++ tool that prints `.md` files _formatted properly_ in your
terminal. This sample exercises ***every*** feature so you can eyeball the
output. Visit the [project home](https://example.com/md) for more, or email
<hello@example.com>.

## Inline styles

You get **bold**, _italic_, ***bold italic***, ~~strikethrough~~, and
`inline code`. Underscores inside identifiers like `some_long_name` stay
literal. Escapes work too: \*not emphasized\* and a literal backtick \` here.

An autolink: <https://en.wikipedia.org/wiki/Markdown>. An image renders as
alt text: ![a small kitten](https://example.com/cat.png).

## Lists

Unordered, with nesting:

- First bullet
- Second bullet
  - Nested child
  - Another child
    - Deeper still
- Back to the top level

Ordered:

1. Preheat the terminal
2. Pipe in some Markdown
3. Admire the output

Task list:

- [x] Parse blocks
- [x] Render tables
- [ ] Add syntax highlighting
- [ ] Conquer the world

## Blockquotes

> "The terminal is the most honest interface."
>
> Nested quotes work too:
>
> > and they stack neatly
> > across multiple lines.

## Code

Fenced code preserves formatting and shows the language label:

```cpp
#include <print>
int main() {
    std::println("Hello, Markdown!");
    return 0;  // no wrapping inside code blocks
}
```

## Tables

Alignment is driven by the GFM delimiter row, and columns measure real display
width — including CJK and emoji:

| Language | Paradigm      | Year | Rating |
|:---------|:-------------:|-----:|:------:|
| C++      | Multi         | 1985 | ★★★★☆ |
| Rust     | Systems       | 2010 | ★★★★★ |
| 日本語    | 自然言語 🗾    |  n/a | ★★★☆☆ |
| Python   | Everything    | 1991 | ★★★★☆ |

---

## Wrapping

This final paragraph is deliberately long so you can confirm that soft-wrapping
respects the terminal width, keeps words intact, and re-flows cleanly when you
resize the window or pass `--width`. Styling such as **bold spans** and
`code spans` survive across wrap boundaries without leaking escape codes.
