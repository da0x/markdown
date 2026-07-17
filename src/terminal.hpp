// terminal.hpp — environment probes for width and TTY-ness.
#pragma once

namespace md {

// Columns available in the terminal: TIOCGWINSZ, then $COLUMNS, then 80.
int terminal_width();

// Is standard output connected to a terminal?
bool stdout_is_tty();

}  // namespace md
