# Makefile

# --------------------------------------------------------------------
# Directories & toolchain
# --------------------------------------------------------------------
SRCDIRS    := src src/core src/audio src/audio/fft src/visualization tools
INC_DIRS   := include $(SRCDIRS)
UNITY_DIR  := tests/vendor/unity

PKG_CONFIG_PATH ?= $(shell brew --prefix)/lib/pkgconfig
PKG_CFLAGS      := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags raylib)
PKG_LIBS        := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs   raylib)

CC      := clang
CFLAGS  := -std=c11 -Wall -Wextra -O3 -march=native \
           $(addprefix -I,$(INC_DIRS)) \
           $(PKG_CFLAGS)
LDFLAGS := $(PKG_LIBS) -lm -lpthread

# --------------------------------------------------------------------
# Sources & Objects
# --------------------------------------------------------------------
SRC       := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c))
# Exclude the bench entrypoint from the main app
SRC       := $(filter-out tools/fft_bench.c,$(SRC))
APP_OBJ   := $(SRC:.c=.o)

# Unity tests
TEST_SRC  := $(wildcard tests/test_*.c)
TEST_OBJ  := $(TEST_SRC:.c=.o) $(UNITY_DIR)/src/unity.o

# Bench tool
BENCH_SRC := tools/fft_bench.c
BENCH_OBJ := tools/fft_bench.o

TARGET    := bragibeats

# --------------------------------------------------------------------
# Phony targets
# --------------------------------------------------------------------
.PHONY: all test clean fft_bench \
	run_tests_audio run_tests_event run_tests_fft run_tests_vis run_tests_fft_verification

# Default build
all: $(TARGET)

$(TARGET): $(APP_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# --------------------------------------------------------------------
# Bench compilation
# --------------------------------------------------------------------
# Compile the bench source
tools/%.o: tools/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link the bench executable
fft_bench: $(BENCH_OBJ) src/audio/fft/fft.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# --------------------------------------------------------------------
# Unity framework
# --------------------------------------------------------------------
# Build Unity once
$(UNITY_DIR)/src/unity.o: $(UNITY_DIR)/src/unity.c
	@echo "Building Unity..."
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------------------------------------------
# Test modules mapping
# --------------------------------------------------------------------
TEST_MODULES_audio_engine := src/audio/audio_engine.o
TEST_MODULES_event_system := src/core/event_system.o
TEST_MODULES_fft   := src/audio/fft/fft.o
TEST_MODULES_visualization_engine   := src/visualization/visualization_engine.o \
                      src/visualization/vis_bars.o \
                      src/visualization/vis_circles.o \
		      src/visualization/vis_utils.o
TEST_MODULES_fft_verification := src/audio/fft/fft.o
TEST_MODULES_vis_bars := src/visualization/vis_bars.o

# Pattern to generate each run_tests_<name> rule
define RUN_TEST
run_tests_$(1): $(UNITY_DIR)/src/unity.o tests/test_$(1).o $$(TEST_MODULES_$(1))
	$(CC) $(CFLAGS) $$^ -o $$@ $(LDFLAGS)
	./$$@
endef

$(eval $(call RUN_TEST,audio_engine))
$(eval $(call RUN_TEST,event_system))
$(eval $(call RUN_TEST,fft))
$(eval $(call RUN_TEST,visualization_engine))
$(eval $(call RUN_TEST,fft_verification))
$(eval $(call RUN_TEST,vis_bars))

test: run_tests_audio_engine run_tests_event_system run_tests_fft run_tests_visualization_engine run_tests_fft_verification run_tests_vis_bars

# --------------------------------------------------------------------
# Generic compilation rule
# --------------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------------------------------------------
# Clean up everything
# --------------------------------------------------------------------
clean:
	rm -rf $(APP_OBJ) $(TEST_OBJ) $(BENCH_OBJ) \
	       $(TARGET) run_tests_* fft_bench

