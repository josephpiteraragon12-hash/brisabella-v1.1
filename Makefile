# Brisabella - compila .exe de 64 y 32 bits con MinGW-w64
#   Linux/Ubuntu:  sudo apt install gcc-mingw-w64-x86-64 gcc-mingw-w64-i686 binutils-mingw-w64
#   Windows:       MSYS2 (mingw-w64-x86_64-gcc / mingw-w64-i686-gcc); ver README
#   Uso:           make          ->  dist/Brisabella-x64.exe  y  dist/Brisabella-x86.exe

NAME := Brisabella
OUT  := dist
BLD  := build

CC64 ?= x86_64-w64-mingw32-gcc
CC32 ?= i686-w64-mingw32-gcc
RC64 ?= x86_64-w64-mingw32-windres
RC32 ?= i686-w64-mingw32-windres

CFLAGS  ?= -Os -Wall -Wextra -std=gnu11 -ffunction-sections -fdata-sections \
           -finput-charset=UTF-8 -fwide-exec-charset=UTF-16LE
LDFLAGS ?= -mwindows -static -s -Wl,--gc-sections -Wl,--dynamicbase -Wl,--nxcompat
LIBS    := -luser32 -lshell32 -ladvapi32 -lgdi32

RES_DEPS := src/brisabella.rc src/brisabella.ico src/brisabella.manifest

.PHONY: all clean
all: $(OUT)/$(NAME)-x64.exe $(OUT)/$(NAME)-x86.exe

$(BLD)/x64/res.o: $(RES_DEPS)
	@mkdir -p $(BLD)/x64
	cd src && $(RC64) -i brisabella.rc -o ../$@

$(BLD)/x86/res.o: $(RES_DEPS)
	@mkdir -p $(BLD)/x86
	cd src && $(RC32) -i brisabella.rc -o ../$@

$(OUT)/$(NAME)-x64.exe: src/main.c $(BLD)/x64/res.o
	@mkdir -p $(OUT)
	$(CC64) $(CFLAGS) src/main.c $(BLD)/x64/res.o -o $@ $(LDFLAGS) $(LIBS)

$(OUT)/$(NAME)-x86.exe: src/main.c $(BLD)/x86/res.o
	@mkdir -p $(OUT)
	$(CC32) $(CFLAGS) src/main.c $(BLD)/x86/res.o -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -rf $(BLD) $(OUT)
