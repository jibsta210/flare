# flare -- plain g++ + pkg-config; no build-system ceremony for six files.
PREFIX ?= /usr/local
DESTDIR ?=
CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra

# Flags required regardless of what the distro passes in.
#
# makepkg REPLACES CXXFLAGS wholesale rather than appending, which silently
# dropped -fPIC and broke every clean package build:
#   copy relocation against non-copyable protected symbol _ZTI7QObject@@Qt_6
# Qt6 exports QObject's typeinfo as a protected symbol, and a non-PIE
# executable cannot emit a copy relocation against one. So -fPIC/-pie are
# appended unconditionally instead of living in the default CXXFLAGS.
REQFLAGS := -std=c++20 -fPIC
REQLDFLAGS := -pie
QTFLAGS := $(shell pkg-config --cflags --libs Qt6Widgets Qt6DBus Qt6Gui Qt6Core)
MOC := $(shell test -x /usr/lib/qt6/moc && echo /usr/lib/qt6/moc || \
         (test -x /usr/lib/qt6/libexec/moc && echo /usr/lib/qt6/libexec/moc || echo moc-qt6))
SRC := src/capture.cpp src/telemetry.cpp src/icons.cpp src/clipboard.cpp \
       src/overlay.cpp src/main.cpp
HDR := src/capture.h src/telemetry.h src/icons.h src/clipboard.h src/overlay.h

all: flare

flare: $(SRC) $(HDR) moc_overlay.cpp
	$(CXX) $(CXXFLAGS) $(REQFLAGS) $(LDFLAGS) $(REQLDFLAGS) -o $@ $(SRC) moc_overlay.cpp $(QTFLAGS)

# Overlay uses Q_OBJECT. NOTE: a bare `moc` on PATH is often Qt5's and fails
# with "generated using the moc from 5.15.x" -- resolve Qt6's explicitly.
moc_overlay.cpp: src/overlay.h
	$(MOC) src/overlay.h -o moc_overlay.cpp

# The Exec= path is LOAD-BEARING. KWin authorizes ScreenShot2 by resolving the
# caller's /proc/PID/exe and matching it against an installed .desktop file's
# Exec line. If they disagree, every capture fails with NoAuthorized -- so the
# desktop file is generated from PREFIX rather than shipped with a fixed path.
flare.desktop: flare.desktop.in
	sed 's|@BINDIR@|$(PREFIX)/bin|' $< > $@

install: flare flare.desktop
	install -Dm755 flare $(DESTDIR)$(PREFIX)/bin/flare
	install -Dm644 flare.desktop $(DESTDIR)$(PREFIX)/share/applications/flare.desktop
	@echo "installed flare to $(PREFIX)/bin/flare"
	@command -v update-desktop-database >/dev/null 2>&1 && \
	  update-desktop-database $(PREFIX)/share/applications 2>/dev/null || true

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/flare
	rm -f $(DESTDIR)$(PREFIX)/share/applications/flare.desktop

clean:
	rm -f flare flare.desktop moc_overlay.cpp

.PHONY: all install uninstall clean
