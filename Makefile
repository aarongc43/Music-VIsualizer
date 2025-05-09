# Makefile

UNITY_DIR := tests/vendor/unity
PKG_CONFIG_PATH ?= $(shell brew --prefix)/lib/pkgconfig
PKG_CFLAGS      := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags raylib)
PKG_LIBS        := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs   raylib)

CC      := clang
CFLAGS  := -std=c11 -Wall -Wextra -O3 -march=native -Iinclude -Isrc -Isrc/core -I$(UNITY_DIR)/src
LDFLAGS := $(PKG_LIBS) -lm -lpthread
SRC := $(filter-out src/tools/fft_bench.c, \
         $(wildcard src/*.c src/*/*.c src/*/*/*.c))
APP_OBJ     := $(SRC:.c=.o)
TARGET  := bragibeats

# Unity test framework path

UNITY_DIR       := tests/vendor/unity
TEST_SRC        := $(wildcard tests/*.c)
TEST_OBJ        := $(TEST_SRC:.c=.o) $(UNITY_DIR)/src/unity.o
TEST_EXE        := run_tests

# Tools:
BENCH_SRC := tools/fft_bench.c
BENCH_OBJ := src/tools/fft_bench.o

.PHONY: all test clean fft_bench

all: $(TARGET)

$(TARGET): $(APP_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build unity once
$(UNITY_DIR)/src/unity.o:
	@echo "Building Unity..."
	$(CC) $(CFLAGS) -c $(UNITY_DIR)/src/unity.c -o $@

# test: event system only 
run_tests_event: $(UNITY_DIR)/src/unity.o tests/test_event_system.o src/core/event_system.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

# test: audio engine only

run_tests_audio: $(UNITY_DIR)/src/unity.o tests/test_audio_engine.o src/audio/audio_engine.o
	$(CC) $(CFLAGS) $^ -o $@
	./$@

run_tests_fft: $(UNITY_DIR)/src/unity.o tests/test_fft.o src/audio/fft/fft.o
	$(CC) $(CFLAGS) $^ -o $@
	./$@

# test runner
test: run_tests_audio run_tests_event run_tests_fft

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) tests/*.o tests/run_tests_* fft_bench

fft_bench: $(BENCH_OBJ) src/audio/fft/fft.o
	$(CC) $(CFLAGS) -o $@ $^ -lm
