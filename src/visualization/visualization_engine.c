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

// Double-buffering: we keep two copies of the processed spectrum so that
// the render thread can safely read from one while the update thread writes
// to the other. g_front_idx is the index (0 or 1) of the buffer currently
// being displayed. The writer always works on the *other* index (1 - g_front_idx).
static float    *g_spectrum[2]  = { NULL, NULL };  // processed spectrum, ready to draw
static float    *g_smooth_state = NULL;            // remembered values for the envelope follower — one continuous history, never double-buffered
static int       g_front_idx = 0;                  // which of the two spectrum buffers is on-screen

// flags to enable/disable stytles
static bool     g_use_bars   = true;

// dB range for the level meter.
// Signals quieter than VIS_MIN_DB are treated as silence (mapped to 0).
// 0 dBFS is the loudest possible digital signal (mapped to 1).
#define VIS_MIN_DB        -85.0f
#define VIS_MAX_DB          0.0f

// Temporal smoothing controls how quickly each bar rises and falls.
// Both values are between 0 and 1 — closer to 1 means the bar holds its
// current value longer (more inertia). Attack controls rising edges;
// decay controls falling edges.
#define VIS_ALPHA_ATTACK    0.85f   // faster rise  (20% of new value blended in per frame)
#define VIS_ALPHA_DECAY     0.95f   // slower fall  ( 5% of new value blended in per frame)

// How many neighboring bins to average together when spatial smoothing.
// 2 means each bin is averaged with the 2 bins to its left and right (5-bin window).
#define VIS_SPATIAL_WINDOW  2

// Keep the shared spectrum data neutral and control visual scale in the renderer.
#define VIS_BARS_MAX_HEIGHT_RATIO 1.0f

bool vis_init(int screen_width, int screen_height, size_t fft_size) {
    if (fft_size == 0) {
        return false;
    }

    g_screen_w = screen_width;
    g_screen_h = screen_height;
    g_bin_count = fft_size / 2;

    // allocate two display buffers for thread-safe swaps
    for (int i = 0; i < 2; ++i) {
        g_spectrum[i] = calloc(g_bin_count, sizeof(float));
        if (!g_spectrum[i]) {
            for (int j = 0; j < i; ++j) {
                free(g_spectrum[j]);
                g_spectrum[j] = NULL;
            }
            return false;
        }
    }

    // single smoothing-state buffer — it tracks every frame continuously,
    // independent of which display buffer is currently active
    g_smooth_state = calloc(g_bin_count, sizeof(float));
    if (!g_smooth_state) {
        free(g_spectrum[0]); g_spectrum[0] = NULL;
        free(g_spectrum[1]); g_spectrum[1] = NULL;
        return false;
    }

    // init sub-renderers; bail if either fails
    if (!bars_full_init(g_bin_count,
                        g_screen_w,
                        g_screen_h,
                        96,
                        2.0f,
                        VIS_BARS_MAX_HEIGHT_RATIO)) return false;

    return true;
}

void vis_update(const float *magnitudes, size_t bin_count) {
    // sanity checks
    if (bin_count != g_bin_count || !magnitudes) return;

    // We write into whichever spectrum buffer is NOT currently on-screen,
    // so the renderer never sees a half-finished frame.
    int   write_idx = 1 - g_front_idx;
    float *write_buf = g_spectrum[write_idx];

    // 1) Raw magnitudes → normalized 0…1 dBFS
    //    Converts linear amplitude to decibels, then maps the result so that
    //    VIS_MIN_DB becomes 0.0 and 0 dBFS becomes 1.0.
    vis_convert_to_dbfs(
        magnitudes,
        write_buf,
        bin_count,
        VIS_MIN_DB,
        VIS_MAX_DB
    );

    // 2) Temporal smoothing (envelope follower)
    //    Each bin rises quickly but falls slowly, like a VU meter needle.
    //    g_smooth_state carries the remembered value forward every frame
    //    without skipping, so the time constants behave as intended.
    vis_apply_temporal_smoothing(
        write_buf,
        g_smooth_state,
        bin_count,
        VIS_ALPHA_ATTACK,
        VIS_ALPHA_DECAY
    );

    // 3) Spatial smoothing (moving average across neighboring bins)
    //    Blurs the spectrum slightly so adjacent bars don't jump independently.
    vis_apply_spatial_smoothing(
        write_buf,
        bin_count,
        VIS_SPATIAL_WINDOW
    );

    // 4) Clamp to [0, 1]. Keep renderer-specific weighting out of here
    //    so each visualizer can decide how to interpret the spectrum.
    for (size_t k = 0; k < bin_count; ++k) {
        if (write_buf[k] > 1.0f) write_buf[k] = 1.0f;
    }

    // 5) Make the buffer we just finished the new on-screen buffer.
    g_front_idx = write_idx;
}

void vis_render(void) {
    const float *read_buf = g_spectrum[g_front_idx];

    if (g_use_bars) bars_full_render(read_buf);
}

void vis_shutdown(void) {
    bars_full_shutdown();
    for (int i = 0; i < 2; ++i) {
        free(g_spectrum[i]);
        g_spectrum[i] = NULL;
    }
    free(g_smooth_state);
    g_smooth_state = NULL;
    g_front_idx = 0;
}
