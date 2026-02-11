SRCDIRS    := src/core src/audio src/audio/fft src/visualization src/ui
INC_DIRS   := src src/core src/audio src/audio/fft src/visualization src/ui
UNITY_DIR  := tests/vendor/unity
OBJ_DIR    := build/obj
BIN_DIR    := build/bin
TEST_BIN_DIR := build/tests

PKG_CONFIG_PATH ?= $(shell brew --prefix)/lib/pkgconfig
PKG_CFLAGS      := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags raylib)
PKG_LIBS        := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs   raylib)

CC      := clang
CFLAGS  := -std=c17 -Wall -Wextra -O3 -march=native \
           $(addprefix -I,$(INC_DIRS)) \
           $(PKG_CFLAGS)
LDFLAGS := $(PKG_LIBS) -lm -lpthread

SRC       := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c))
APP_OBJ   := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))

# Unity tests
TEST_NAMES := audio_engine event_system fft visualization_engine fft_verification music_library
TEST_SRC   := $(addprefix tests/test_,$(addsuffix .c,$(TEST_NAMES)))
UNITY_OBJ  := $(OBJ_DIR)/$(UNITY_DIR)/src/unity.o
TEST_OBJ   := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRC)) $(UNITY_OBJ)
TEST_BINS  := $(addprefix run_tests_,$(TEST_NAMES))

TARGET    := $(BIN_DIR)/visualizer

.PHONY: all run test clean $(TEST_BINS)

# Default build
all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(APP_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(UNITY_OBJ): $(UNITY_DIR)/src/unity.c
	@echo "Building Unity..."
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

TEST_MODULES_audio_engine := $(OBJ_DIR)/src/audio/audio_engine.o
TEST_MODULES_event_system := $(OBJ_DIR)/src/core/event_system.o
TEST_MODULES_fft          := $(OBJ_DIR)/src/audio/fft/fft.o
TEST_MODULES_visualization_engine := $(OBJ_DIR)/src/visualization/visualization_engine.o \
	                                    $(OBJ_DIR)/src/visualization/vis_bars_full.o \
	                                    $(OBJ_DIR)/src/visualization/vis_utils.o
TEST_MODULES_fft_verification := $(OBJ_DIR)/src/audio/fft/fft.o
TEST_MODULES_music_library := $(OBJ_DIR)/src/audio/music_library.o
# Removed legacy vis_bars / vis_circles tests

define RUN_TEST
run_tests_$(1): $(UNITY_OBJ) $(OBJ_DIR)/tests/test_$(1).o $$(TEST_MODULES_$(1))
	@mkdir -p $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) $$^ -o $(TEST_BIN_DIR)/$$@ $(LDFLAGS)
	./$(TEST_BIN_DIR)/$$@
endef

$(foreach test_name,$(TEST_NAMES),$(eval $(call RUN_TEST,$(test_name))))

test: $(TEST_BINS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build visualizer run_tests_*
