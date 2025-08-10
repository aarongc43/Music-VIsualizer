// src/visualization/visualization_engine.c 

#include "visualization_engine.h"
#include "vis_bars_full.h"
#include "vis_utils.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// private state, hidden from the header

static int      g_screen_w = 0;
static int      g_screen_h = 0;
static size_t   g_bin_count = 0;

// double buffer so render and update never race
static float    *g_vis_buf[2]       = { NULL, NULL };
static float    *g_smooth_prev[2]   = { NULL, NULL };
static int       g_front = 0;

// flags to enable/disable stytles
static bool     g_use_bars   = true;

// dB range for your meter
#define VIS_MIN_DB        -85.0f  // Lower min dB for better dynamic range
#define VIS_MAX_DB          0.0f

// temporal smoothing: lower α_attack → faster rise; higher α_decay → slower fall
#define VIS_ALPHA_ATTACK    0.80f   // slower rise
#define VIS_ALPHA_DECAY     0.95f   // slower fall

// spatial smoothing window (1 = average of k−1, k, k+1)
#define VIS_SPATIAL_WINDOW  2      // Wider window for smoother transitions between bins

bool vis_init(int screen_width, int screen_height, size_t fft_size) {
    if (fft_size == 0) {
        return false;
    }

    g_screen_w = screen_width;
    g_screen_h = screen_height;
    g_bin_count = fft_size / 2;

    // allocate two buffers for thread-safe swaps
    for (int i = 0; i < 2; ++i) {
        g_vis_buf[i] = calloc(g_bin_count, sizeof(float));
        g_smooth_prev[i] = calloc(g_bin_count, sizeof(float));

        if (!g_vis_buf[i]) {
            // cleanup an that succeeded
            for (int j = 0; j < i; ++j) {
                free(g_vis_buf[j]);
                g_vis_buf[j] = NULL;
            }
            return false;
        }
    }

    // init sub-renderers; bail if either fails
    if (!bars_full_init(g_bin_count, g_screen_w, g_screen_h, 96, 2.0f, 4.0f)) return false;

    return true;
}

void vis_update(const float *magnitudes, size_t bin_count) {
    // sanity checks
    if (bin_count != g_bin_count || !magnitudes) return;

    // flip to the “back” buffer
    int back = 1 - g_front;
    float *dst = g_vis_buf[back];

    // 1) Raw magnitudes → normalized 0…1 dBFS
    vis_convert_to_dbfs(
        magnitudes,
        dst,
        bin_count,
        VIS_MIN_DB,
        VIS_MAX_DB
    );

    // 2) Temporal smoothing (envelope follower)
    //    in-place on dst[], using previous state in g_smooth_prev[back][]
    vis_apply_temporal_smoothing(
        dst,
        g_smooth_prev[back],
        bin_count,
        VIS_ALPHA_ATTACK,
        VIS_ALPHA_DECAY
    );

    // 3) Spatial smoothing (moving average)
    vis_apply_spatial_smoothing(
        dst,
        bin_count,
        VIS_SPATIAL_WINDOW
    );

    // 4) Ease-out cubic for a more organic curve
    for (size_t k = 0; k < bin_count; ++k) {
        float w = sqrtf((k+1) / (float)bin_count);
        float v = g_vis_buf[back][k] * w;
        g_vis_buf[back][k] = v > 1.0f ? 1.0f : v;
    }

    // 5) Commit the flip
    g_front = back;
}

void vis_render(void) {
    const float *buf = g_vis_buf[g_front];

    if (g_use_bars)     bars_full_render(buf);
}

void vis_shutdown(void) {
    bars_full_shutdown();
    for (int i = 0; i < 2; ++i) {
        free(g_vis_buf[i]);
        g_vis_buf[i] = NULL;
    }
    g_front = 0;
}
