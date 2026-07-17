# md

A small, dependency-free command-line tool that renders Markdown `.md` files
formatted properly in your terminal — headings, lists, tables, code blocks,
blockquotes, and inline styling, all wrapped to your terminal width.

Written in modern C++ (C++23), no third-party libraries.

## Features

- **Headings** with per-level color and an underline rule for H1/H2
- **Inline styles**: `**bold**`, `_italic_`, `***both***`, `~~strike~~`,
  `` `code` ``, backslash escapes
- **Links** as clickable [OSC 8](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda)
  hyperlinks (falling back to `text (url)` when piped)
- **Lists**: ordered, unordered, nested, and `- [ ]` / `- [x]` task lists
- **Blockquotes**, including nested ones, with a colored bar
- **Fenced code blocks** with a gutter, language label, and **syntax
  highlighting** for C, C++, Python, JavaScript/TypeScript, Rust, Go, shell, and
  JSON (unknown languages still get string/number highlighting)
- **GFM tables** with box-drawing borders, per-column alignment, and correct
  display-width measurement for CJK/emoji — wide tables shrink to fit
- **Word-wrapping** to the terminal width, ANSI-aware so styles never leak
- Auto-disables color and wraps to 80 columns when output is piped

## Build & install

Requires a C++23 compiler (GCC 13+/Clang 16+). No third-party libraries.

```sh
make                 # builds ./md
make test            # renders README.md and test/sample.md
sudo make install    # copies md to /usr/local/bin
sudo make uninstall  # removes it
```

Install elsewhere with `make install PREFIX=$HOME/.local` (or package into a
staging root with `make install DESTDIR=/tmp/pkg`).

A CMake build is also provided:

```sh
cmake -B build && cmake --build build   # binary at build/md
```

## Usage

```
md [OPTIONS] [FILE]

  With no FILE (or "-"), reads Markdown from standard input.

OPTIONS:
  -w, --width N     Wrap to N columns (default: terminal width)
      --color       Force ANSI color/hyperlinks on
      --no-color    Disable ANSI color/hyperlinks
  -h, --help        Show this help
  -v, --version     Show version
```

### Examples

```sh
md README.md                 # render a file
md --width 100 notes.md      # fixed wrap width
cat CHANGELOG.md | md         # from a pipe
md test/sample.md            # a feature showcase
```

## Project layout

| File | Responsibility |
|------|----------------|
| `src/unicode.hpp` | UTF-8 decoding and display-width measurement |
| `src/ansi.hpp` | ANSI SGR styles and OSC 8 hyperlinks |
| `src/text_util.hpp` | ANSI-aware width and truncation |
| `src/ast.hpp` | Inline and block node definitions |
| `src/inline_parser.*` | Emphasis, code, links, images, autolinks |
| `src/block_parser.*` | Headings, lists, quotes, code, tables, paragraphs |
| `src/highlight.*` | Syntax highlighting for fenced code blocks |
| `src/table.hpp` | Box-drawn tables (style-as-data, alignment, shrink) |
| `src/renderer.*` | AST → ANSI, word-wrapping, line prefixing |
| `src/terminal.*` | Terminal width and TTY detection |
| `src/main.cpp` | CLI |

## License

See [LICENSE](LICENSE).
