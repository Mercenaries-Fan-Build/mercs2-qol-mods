# Makefile for quiet_freeplay_vo.asi — mute/throttle freeplay nag & proximity VO
#
# Cross-compiles from macOS/Linux using MinGW, or native on Windows with MSVC.
#
# Prerequisites:
#   macOS:   brew install mingw-w64
#   Ubuntu:  apt install gcc-mingw-w64-i686
#   Windows: Visual Studio with C++ Desktop workload

CC_MINGW    = i686-w64-mingw32-gcc
STRIP_MINGW = i686-w64-mingw32-strip
CC_MSVC     = cl

TARGET      = quiet_freeplay_vo.asi
SRC         = quiet_freeplay_vo.c
CFLAGS      = -O2 -Wall -shared -Wl,--enable-stdcall-fixup $(EXTRA_CFLAGS)
LDFLAGS     = -lkernel32 -luser32

.PHONY: all clean mingw msvc help

all: mingw

mingw: $(SRC)
	$(CC_MINGW) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	$(STRIP_MINGW) $(TARGET)
	@echo "Built: $(TARGET) ($$(wc -c < $(TARGET)) bytes)"
	@echo "Copy to: <game>/scripts/  (along with quiet_freeplay_vo.ini)"

msvc: $(SRC)
	$(CC_MSVC) /LD /O2 /GS- $(SRC) /link /OUT:$(TARGET) kernel32.lib user32.lib
	@echo "Built: $(TARGET)"

clean:
	rm -f $(TARGET) quiet_freeplay_vo.obj quiet_freeplay_vo.exp quiet_freeplay_vo.lib quiet_freeplay_vo.log

help:
	@echo "Usage:"
	@echo "  make mingw   — Cross-compile with MinGW (default)"
	@echo "  make msvc    — Compile with MSVC (Windows)"
	@echo "  make clean   — Remove build artifacts"
