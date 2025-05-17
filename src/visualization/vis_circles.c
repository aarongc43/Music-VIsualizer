// src/visualization/vis_circles.c 

#include "vis_circles.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#define VIS_MAX_MAG         10000.0f;
#define VIS_CIRCLE_COLOR    RED
#define VIS_CIRCLE_STEPS    8

static Vector2  *g_centers      = NULL;
static size_t   g_circle_count = 0;
static int      g_win_w         = 0;
static int      g_win_h         = 0;
static float    g_max_radius    = 0.0f;

bool cirlces_init(size_t bin_count, int screen_w, int screen_h) {
    g_circle_count  = bin_count;
    g_win_w         = screen_w;
    g_win_h         = screen_h;
    g_max_radius    = fminf((float)g_win_w, (float)g_win_h) * 0.4f;

    g_centers = calloc(g_circle_count, sizeof(Vector2));
    if (!g_centers) return false;

    Vector2 center = { g_win_w * 0.5f, g_win_h * 0.5f };
    for (size_t i = 0; i < g_circle_count; ++i) {
        g_centers[i] = center;
    }
    return true;
}

void circles_render(const float *magnitudes) {
    if (!g_centers || !magnitudes) return;

    size_t step = g_circle_count / VIS_CIRCLE_STEPS;
    if (step == 0) step = 1;

    for (size_t i = 0; i < g_circle_count; i += step) {
        // normalize and clamp
        float norm = magnitudes[i] / VIS_MAX_MAG;
        if (norm > 1.0f) norm = 1.0f;

        // radius scales with both magnitudes and its relative bin index
        float radius = norm * g_max_radius * ((float)i / (float)g_circle_count);

        DrawCircleV(g_centers[i], radius, VIS_CIRCLE_COLOR);
    }
}

void circles_shutdown(void) {
    free(g_centers);
    g_centers = NULL;
}
