// src/visualization/visualization_engine.c 

#include "visualization_engine.h"
#include "vis_bars.h"
#include "vis_circles.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// private state, hidden from the header

static int      g_screen_w = 0;
static int      g_screen_h = 0;
static size_t   g_bin_count = 0;

// double buffer so render and update never race
static float    *g_vis_buf[2] = { NULL, NULL };
static int       g_front = 0;

// flags to enable/disable stytles
static bool     g_use_bars      = true;
static bool     g_use_circles   = true;

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
    if (!bars_init(g_bin_count, g_screen_w, g_screen_h)) return false;
    if (!circles_init(g_bin_count, g_screen_w, g_screen_h)) return false;

    return true;
}

void vis_update(const float *magnitudes, size_t bin_count) {
    if (bin_count != g_bin_count || !magnitudes) return;
    // copy into back buffer, then flip
    int back = 1 - g_front;
    memcpy(g_vis_buf[back], magnitudes, bin_count * sizeof(float)); 
    g_front = back;
}

void vis_render(void) {
    const float *buf = g_vis_buf[g_front];
    if (g_use_bars)     bars_render(buf);
    if (g_use_circles)  circles_render(buf);
}

void vis_shutdown(void) {
    bars_shutdown();
    circles_shutdown();
    for (int i = 0; i < 2; ++i) {
        free(g_vis_buf[i]);
        g_vis_buf[i] = NULL;
    }
    g_front = 0;
}
