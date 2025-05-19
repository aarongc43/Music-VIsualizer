// src/visualization/vis_bars.c

#include "vis_bars.h"

#include <stdlib.h>
#include <raylib.h>

#define VIS_BAR_COLOR BLUE    // single-color bars; you can swap for a gradient

static Rectangle *g_bar_recs   = NULL;
static size_t     g_bar_count  = 0;
static int        g_win_w      = 0;
static int        g_win_h      = 0;
static float      g_max_height = 0.0f;

bool bars_init(size_t bin_count, int screen_w, int screen_h) {
    g_bar_count  = bin_count;
    g_win_w      = screen_w;
    g_win_h      = screen_h;
    g_max_height = screen_h * 0.8f;

    g_bar_recs = calloc(g_bar_count, sizeof(Rectangle));
    if (!g_bar_recs) return false;

    float bar_width = (float)g_win_w / (float)g_bar_count;
    for (size_t i = 0; i < g_bar_count; ++i) {
        g_bar_recs[i].x      = i * bar_width;
        g_bar_recs[i].width  = bar_width - 1;
        g_bar_recs[i].y      = g_win_h;
        g_bar_recs[i].height = 0;
    }
    return true;
}

void bars_render(const float *magnitudes) {
    if (!g_bar_recs || !magnitudes) return;

    // 1) Find the maximum magnitude this frame
    float max_mag = 0.0f;
    for (size_t i = 0; i < g_bar_count; ++i) {
        if (magnitudes[i] > max_mag) {
            max_mag = magnitudes[i];
        }
    }
    if (max_mag <= 1e-6f) {
        // all zeros or too small – nothing to draw
        return;
    }

    // 2) Draw each bar scaled relative to that max
    for (size_t i = 0; i < g_bar_count; ++i) {
        float norm = magnitudes[i] / max_mag;
        // clamp [0,1]
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        float h = norm * g_max_height;
        g_bar_recs[i].height = h;
        g_bar_recs[i].y      = g_win_h - h;

        DrawRectangleRec(g_bar_recs[i], VIS_BAR_COLOR);
    }
}

void bars_shutdown(void) {
    free(g_bar_recs);
    g_bar_recs = NULL;
}

