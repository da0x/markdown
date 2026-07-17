// terminal.cpp — POSIX terminal probes.
#include "terminal.hpp"

#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

namespace md {

int terminal_width() {
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    if (const char* cols = std::getenv("COLUMNS")) {
        try {
            int n = std::stoi(cols);
            if (n > 0) return n;
        } catch (...) {
        }
    }
    return 80;
}

bool stdout_is_tty() {
    return isatty(STDOUT_FILENO) == 1;
}

}  // namespace md
