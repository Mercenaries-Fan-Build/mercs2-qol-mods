# Makefile for loadmarkers.asi — built against the Mercs2 mod stdlib (sdk/).
#
# Prerequisites:
#   macOS:   brew install mingw-w64
#   Ubuntu:  apt install gcc-mingw-w64-i686

include ../../sdk/sdk.mk

CC_MINGW    = i686-w64-mingw32-gcc
STRIP_MINGW = i686-w64-mingw32-strip
CC_MSVC     = cl

TARGET      = loadmarkers.asi
SRC         = loadmarkers.c
CFLAGS      = -O2 -Wall -Wno-unused-function -shared -Wl,--enable-stdcall-fixup $(M2_CFLAGS) $(EXTRA_CFLAGS)
LDFLAGS     = -lkernel32 -luser32

.PHONY: all clean mingw msvc help

all: mingw

mingw: $(SRC)
	$(CC_MINGW) $(CFLAGS) -o $(TARGET) $(SRC) $(M2_SRCS) $(LDFLAGS)
	-$(STRIP_MINGW) $(TARGET)
	@echo "Built: $(TARGET) ($$(wc -c < $(TARGET)) bytes)"
	@echo "Copy to: <game>/scripts/"

msvc: $(SRC)
	$(CC_MSVC) /LD /O2 /GS- $(M2_CFLAGS) $(SRC) $(M2_SRCS) /link /OUT:$(TARGET) kernel32.lib user32.lib
	@echo "Built: $(TARGET)"

clean:
	rm -f $(TARGET) *.obj *.exp *.lib loadmarkers.log

help:
	@echo "make mingw | msvc | clean"
