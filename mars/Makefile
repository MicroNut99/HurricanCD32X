# Hurrican CD32X - the SH2 program (P1: stage 1, playable), linked as a 32X cartridge image
# for the CD32X loader.
#   make -C mars
# Results: mars/hurrican32x.elf / .bin / .map, mars/OPENJAZZ.32X, mars/report.txt (sizes).
#
# The start-up and platform layer are the OpenJazz port's J19b files (the last build proven on
# the console), unchanged, compiled with exactly the flags of the J19b engine build (PORT_*):
# port/mars/crt0_oj.s, port/mars/oj.ld, port/plat_mars.c, port/mars_syscalls.c, port/pack.c,
# port/mars_boottest.cpp, and tools/make_oj_rom.py for the cartridge image. Only game/p1.c
# is new; it runs after main() is called, after the boot path has finished.
# build.sh passes EXTRA_DEFS=-DOJ32X_MUSIC=1 (the J19b engine's music build: CD audio tracks).

TC      ?= /opt/toolchains/sega/sh-elf/bin/sh-elf-
CC      := $(TC)gcc
CXX     := $(TC)g++
SIZE    := $(TC)size
ARCH    ?= -m2 -mb

ROOT  := ..
PORT  := $(ROOT)/port
GAME  := $(ROOT)/game

OBJDIR := obj

# ---- the J19b engine build's settings (mars/Makefile of the OpenJazz port, J19b) ----
PORT_OPT  := $(ARCH) -Os -ffunction-sections -fdata-sections
PORT_DEFS := -DOJ32X -DOJ32X_MARS -DOJ_SDL1 -DPORTABLE -DNDEBUG \
             -DOJ_VERSION=\"git-oj32x\" -DOJ_DATE=\"2026-10-06\" -D__32X__ -DMARS \
             -DOJ_URL=\"http://alister.eu/jazz/oj/\" -DOJ_BUGREPORT=\"https://github.com/AlisterT/openjazz/issues\" \
             -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC -DXMP_SYM_VISIBILITY \
             -DOJ32X_SCROLL -DOJ32X_FAST_CODE -DOJ32X_PANEL_DIFF -DOJ32X_FLIP_ASYNC -DOJ32X_SLAVE_DRAWS=1 $(EXTRA_DEFS)
PORT_INCS := -I$(PORT)/include -I$(PORT)
PORT_CXXFLAGS := -std=gnu++14 $(PORT_OPT) $(PORT_DEFS) $(PORT_INCS) -fno-threadsafe-statics -Wall -Wno-unused-result -Wno-narrowing -MMD -MP
PORT_CFLAGS   := -std=gnu11 $(PORT_OPT) $(PORT_DEFS) $(PORT_INCS) -w -MMD -MP

# ---- the game (new): speed-optimised, the same switches ----
GAME_CFLAGS := -std=gnu11 $(ARCH) -O2 -ffunction-sections -fdata-sections $(PORT_DEFS) $(PORT_INCS) -Wall -MMD -MP

PORT_OBJS := $(OBJDIR)/plat_mars.o $(OBJDIR)/mars_syscalls.o $(OBJDIR)/pack.o $(OBJDIR)/mars_boottest.o
OBJS := $(OBJDIR)/p1.o $(PORT_OBJS)
LD_SCRIPT := $(PORT)/mars/oj.ld

all: OPENJAZZ.32X report.txt
	@echo; cat report.txt

# linked as the J19b engine is linked
hurrican32x.elf: $(OBJDIR)/crt0_oj.o $(OBJS) $(LD_SCRIPT)
	$(CXX) $(ARCH) -nostartfiles -T $(LD_SCRIPT) -Wl,--gc-sections -Wl,-Map,hurrican32x.map \
		-o $@ $(OBJDIR)/crt0_oj.o $(OBJS) -lstdc++ -lm -lc -lgcc

hurrican32x.bin: hurrican32x.elf
	$(TC)objcopy -O binary -j .sdcode -j .data -j .text $< $@

OPENJAZZ.32X: hurrican32x.elf hurrican32x.bin $(ROOT)/tools/make_oj_rom.py $(ROOT)/tools/mars_boot.bin
	python3 $(ROOT)/tools/make_oj_rom.py $(ROOT)/tools/mars_boot.bin hurrican32x.elf hurrican32x.bin $@

report.txt: hurrican32x.elf
	@{ echo "== SH2 sections (bytes)"; $(SIZE) -A hurrican32x.elf | grep -E "sdcode|data|text|bss|Total"; } > $@

$(OBJDIR)/crt0_oj.o: $(PORT)/mars/crt0_oj.s
	@mkdir -p $(OBJDIR)
	$(CC) $(ARCH) -c $< -o $@

$(OBJDIR)/p1.o: $(GAME)/p1.c
	@mkdir -p $(OBJDIR)
	$(CC) $(GAME_CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: $(PORT)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(PORT_CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: $(PORT)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(PORT_CXXFLAGS) -c $< -o $@

clean:
	rm -rf obj hurrican32x.elf hurrican32x.bin hurrican32x.map OPENJAZZ.32X report.txt
.PHONY: all clean

$(OBJS) $(OBJDIR)/crt0_oj.o: Makefile
-include $(OBJS:.o=.d)
