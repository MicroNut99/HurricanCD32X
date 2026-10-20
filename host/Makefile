# Hurrican CD32X - the game on the PC (game/host.c stands in for the 32X's platform layer):
#   make -C host
#   HB_DIR=<folder with IMAGE8.RAW and OJ001.PAK> HB_OUT=/tmp/frames HB_FRAMES=300 HB_SAVE_EVERY=30 \
#     HB_PAD="0:-,60:RIGHT,90:RIGHT+C" host/hurrican_p1_host
# Frames are written as PPM pictures of what the 32X would show (224 lines through the line table).
# make -C host asan: the same with the address and undefined-behaviour checkers.
# K2=<group> builds with only that group of added enemy kinds (game/k2/<group>_list.h and
# <group>.c); OUT=<folder> puts the programs there.
CC     ?= gcc
OUT    ?= .
CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -I../port -DOJ32X
ifneq ($(K2),)
CFLAGS += -DK2_LIST='"k2/$(K2)_list.h"' -DK2_CODE='"k2/$(K2).c"'
endif
SRC    := ../game/p1.c ../game/host.c ../port/pack.c
DEPS   := $(wildcard ../game/*.c ../game/*.h ../game/k2/*.c ../game/k2/*.h)
all: $(OUT)/hurrican_p1_host
$(OUT)/hurrican_p1_host: $(SRC) $(DEPS) ../port/pack.h ../port/plat.h
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ $(SRC)
asan: $(SRC) $(DEPS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize=pointer-overflow -o $(OUT)/hurrican_p1_asan $(SRC)
clean:
	rm -f hurrican_p1_host hurrican_p1_asan hurrican_b1_host
.PHONY: all clean asan
reach: ../game/reach.c ../game/p1.c ../game/host.c ../port/pack.c
	$(CC) $(CFLAGS) -o hurrican_p1_reach ../game/reach.c ../game/host.c ../port/pack.c
.PHONY: reach
demorec: ../game/demorec.c ../game/p1.c ../game/host.c ../port/pack.c $(DEPS)
	$(CC) $(CFLAGS) -o hurrican_demorec ../game/demorec.c ../game/host.c ../port/pack.c
.PHONY: demorec
