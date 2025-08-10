#pragma once
#include <stddef.h>
#include <stdbool.h>

// Initialize a full-width bars visualizer
// bin_count
// screen width & height
// bar_count
// bar spacing
// max_height_ratio

bool bars_full_init(size_t bin_count,
                    int screen_w,
                    int screen_h,
                    size_t bar_count,
                    float spacing_px,
                    float max_height_ratio);
void bars_full_render(const float *norm_bins_0_to1);
void bars_full_shutdown(void);
