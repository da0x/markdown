// main.cpp — the `md` command-line entry point.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ansi.hpp"
#include "block_parser.hpp"
#include "renderer.hpp"
#include "terminal.hpp"

namespace {

constexpr const char* kVersion = "md 0.1.0";

void print_help() {
    std::cout <<
        R"(md — render Markdown in your terminal

USAGE:
    md [OPTIONS] [FILE]

    With no FILE (or "-"), reads Markdown from standard input.

OPTIONS:
    -w, --width N     Wrap to N columns (default: terminal width)
        --color       Force ANSI color/hyperlinks on
        --no-color    Disable ANSI color/hyperlinks
    -h, --help        Show this help
    -v, --version     Show version

EXAMPLES:
    md README.md
    md --width 100 notes.md
    cat CHANGELOG.md | md -
)";
}

std::string read_stream(std::istream& in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
    std::string file;
    int width = 0;         // 0 → auto
    int color_mode = 0;    // 0 auto, 1 force on, -1 force off

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "-h" || a == "--help") {
            print_help();
            return 0;
        }
        if (a == "-v" || a == "--version") {
            std::cout << kVersion << "\n";
            return 0;
        }
        if (a == "--color") {
            color_mode = 1;
        } else if (a == "--no-color") {
            color_mode = -1;
        } else if (a == "-w" || a == "--width") {
            if (i + 1 >= argc) {
                std::cerr << "md: " << a << " requires an argument\n";
                return 2;
            }
            try {
                width = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "md: invalid width\n";
                return 2;
            }
        } else if (a.starts_with("--width=")) {
            try {
                width = std::stoi(std::string(a.substr(8)));
            } catch (...) {
                std::cerr << "md: invalid width\n";
                return 2;
            }
        } else if (a == "-") {
            file.clear();
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "md: unknown option '" << a << "' (try --help)\n";
            return 2;
        } else {
            file = a;
        }
    }

    std::string source;
    if (file.empty()) {
        source = read_stream(std::cin);
    } else {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            std::cerr << "md: cannot open '" << file << "': "
                      << std::strerror(errno) << "\n";
            return 1;
        }
        source = read_stream(in);
    }

    bool tty = md::stdout_is_tty();
    md::g_color_enabled = color_mode == 1 || (color_mode == 0 && tty);
    if (width <= 0) width = tty ? md::terminal_width() : 80;

    auto blocks = md::parse_document(source);
    std::string out = md::render_document(blocks, width);
    if (!out.empty()) std::cout << out << "\n";
    return 0;
}
