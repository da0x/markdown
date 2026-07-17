# md — terminal Markdown renderer
#
# Common targets:
#   make            build the ./md binary
#   make test       render README.md and the feature showcase
#   make install    copy md to $(PREFIX)/bin        (default /usr/local, needs sudo)
#   make uninstall  remove the installed md
#   make clean      remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++23 -O2 -Wall -Wextra -Wpedantic -Wno-missing-field-initializers
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin

BIN      := md
SRCDIR   := src
OBJDIR   := obj
SRCS     := $(wildcard $(SRCDIR)/*.cpp)
OBJS     := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

.PHONY: all test install uninstall clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -MMD -MP -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Render bundled Markdown so you can eyeball the output.
test: $(BIN)
	./$(BIN) README.md
	./$(BIN) test/sample.md

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	@echo "installed $(BIN) -> $(DESTDIR)$(BINDIR)/$(BIN)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	@echo "removed $(DESTDIR)$(BINDIR)/$(BIN)"

clean:
	rm -rf $(OBJDIR) $(BIN)

-include $(DEPS)
