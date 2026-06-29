# Makefile for multiplayer_restore.asi — built against the Mercs2 mod stdlib (sdk/).
#
# Drop this directory under mercs2-qol-mods/mods/multiplayer-restore/ and run
# `make`. For an out-of-tree checkout, point SDK_DIR at the SDK folder:
#   make SDK_DIR=/path/to/mercs2-qol-mods/sdk
#
# Prerequisites:
#   macOS:   brew install mingw-w64
#   Ubuntu:  apt install gcc-mingw-w64-i686
#   Windows: MinGW (i686) on PATH, or MSVC via `make msvc`

SDK_DIR ?= ../../sdk
include $(SDK_DIR)/sdk.mk

CC_MINGW    = i686-w64-mingw32-gcc
STRIP_MINGW = i686-w64-mingw32-strip
CC_MSVC     = cl

TARGET      = multiplayer_restore.asi
SRC         = multiplayer_restore.c
# -static-libgcc: the time-spoof's 64-bit division pulls in libgcc's
# __udivdi3; static-link it so the .asi has no libgcc_s_dw2-1.dll runtime dep.
CFLAGS      = -O2 -Wall -Wno-unused-function -shared -static-libgcc \
              -Wl,--enable-stdcall-fixup $(M2_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS     = -lkernel32 -luser32 -lws2_32 -lwintrust

.PHONY: all clean mingw msvc help

all: mingw

mingw: $(SRC)
	$(CC_MINGW) $(CFLAGS) -o $(TARGET) $(SRC) $(M2_SRCS) $(LDFLAGS)
	-$(STRIP_MINGW) $(TARGET)
	@echo "Built: $(TARGET) ($$(wc -c < $(TARGET)) bytes)"
	@echo "Copy to: <game>/scripts/  (along with multiplayer_restore.ini)"

msvc: $(SRC)
	$(CC_MSVC) /LD /O2 /GS- $(M2_CFLAGS) $(SRC) $(M2_SRCS) \
		/link /OUT:$(TARGET) kernel32.lib user32.lib ws2_32.lib wintrust.lib
	@echo "Built: $(TARGET)"

clean:
	rm -f $(TARGET) *.obj *.exp *.lib multiplayer_restore.log

help:
	@echo "make mingw | msvc | clean"
