// src/visualization/vis_bars.h

#ifndef VIS_BARS_H
#define VIS_BARS_H

#include <stddef.h>
#include <stdbool.h>
#include <raylib.h>

// Default bar-count and spacing for the 3-arg init wrapper:
#ifndef BAR_COUNT
#define BAR_COUNT 64
#endif

#ifndef BAR_SPACING
#define BAR_SPACING 2.0f
#endif

// Minimum bar height for visibility
#ifndef BAR_MIN_H
#define BAR_MIN_H 2.0f
#endif

// Define a threshold for noise filtering (not used in tests)
#ifndef NOISE_THRESHOLD
#define NOISE_THRESHOLD 0.01f
#endif

/**
 * Initialize the bar renderer.
 * @param bin_count   Number of FFT bins (must match fft_size/2).
 * @param screen_w    Width of the window in pixels.
 * @param screen_h    Height of the window in pixels.
 * @param bar_count   How many bars you want visible (e.g. 64).
 * @param spacing_px  Pixels between bars.
 * @return            true on success, false on allocation error.
 */
bool bars_init_full(size_t bin_count,
               int screen_w,
               int screen_h,
               size_t bar_count,
               float spacing_px);

// 3-arg init wrapper for tests: uses BAR_COUNT and BAR_SPACING
bool bars_init(size_t bin_count,
               int screen_w,
               int screen_h);

/**
 * Render one frame of bars.
 * @param vis_data  Array of length bin_count of normalized [0…1] values.
 */
void bars_render(const float *vis_data);

/** Free all internal buffers. */
void bars_shutdown(void);

// Unit-testable compute: write each bar’s height into `heights[i]`
void vis_bars_compute(const float *mag,
                      float *heights);

#endif /* VIS_BARS_H */

