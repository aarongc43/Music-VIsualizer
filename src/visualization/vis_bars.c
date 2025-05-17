// src/visualization/vis_bars.c 

#include "vis_bars.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define VIS_MAX_MAG 10000.0f // normalization constant for magnitudes
#define VIS_BAR_COLOR BLUE

static Rectangle    *g_bar_recs     = NULL;
static size_t       g_bar_count     = 0;
static int          g_win_w         = 0;
static int          g_win_h         = 0;
static float        g_max_height    = 0.0f;

bool bars_init(size_t bin_count, int screen_w, int screen_h) {
    g_bar_count     = bin_count;
    g_win_w         = screen_w;
    g_win_h         = screen_h;
    g_max_height    = screen_h * 0.8f;

    g_bar_recs = calloc(g_bar_count, sizeof(Rectangle));
    if (!g_bar_recs) return false;

    float bar_width = (float)g_win_w / (float)g_bar_count;
    for (size_t i = 0; i < g_bar_count; ++i) {
        g_bar_recs[i].x     = i * bar_width;
        g_bar_recs[i].width = bar_width - 1;
        g_bar_recs[i].y     =g_win_h;
        g_bar_recs[i].height = 0;
    }

    return true;
}

void bars_render(const float *magnitudes) {
    if (!g_bar_recs || !magnitudes) return;

    for (size_t i = 0; i < g_bar_count; ++i) {
        // normalize and clamp
        float norm = magnitudes[i] / VIS_MAX_MAG;
        if (norm > 1.0f) norm = 1.0f;

        // compute height & y-position
        float h = norm * g_max_height;
        g_bar_recs[i].height = h;
        g_bar_recs[i].y         = g_win_h - h;

        DrawRectangleRec(g_bar_recs[i],VIS_BAR_COLOR);
    }
}

void bars_shutdown(void) {
    free(g_bar_recs);
    g_bar_recs = NULL;
}
