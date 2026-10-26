CC      ?= clang
CFLAGS  = -std=c99 -O2 -Wall -Wextra -pedantic -I../game
GAME    = ../game/ur_rules.c ../game/ur_ai.c
HDRS    = ../game/ur_rules.h ../game/ur_ai.h
BUILD   = build
ARMGCC  = /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_main: test_main.c $(GAME) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ test_main.c $(GAME) -lm

$(BUILD)/ladder: ladder.c $(GAME) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ ladder.c $(GAME) -lm

$(BUILD)/xcheck_gen: ../tools/xcheck/gen.c $(GAME) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ ../tools/xcheck/gen.c $(GAME)

test: $(BUILD)/test_main
	$(BUILD)/test_main

ladder: $(BUILD)/ladder
	$(BUILD)/ladder all 2000

xcheck: $(BUILD)/xcheck_gen
	../tools/xcheck/run.sh

# Compile the game code with the 3DS toolchain (warnings only, no link)
arm:
	$(ARMGCC) -std=gnu99 -O2 -Wall -Wextra -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -I../game -c ../game/ur_rules.c -o $(BUILD)/arm_rules.o
	$(ARMGCC) -std=gnu99 -O2 -Wall -Wextra -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -I../game -c ../game/ur_ai.c -o $(BUILD)/arm_ai.o

clean:
	rm -rf $(BUILD)

.PHONY: all test ladder xcheck arm clean
