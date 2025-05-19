// src/visualization/vis_bars.c

#include "vis_bars.h"

#include <raylib.h>
#include <stdlib.h>
#include <math.h>

/*
 * Situation:
 *   Our current bar visualizer looks blocky and uses a fixed-rate EMA,
 *   resulting in a “jumpy,” non-natural feel.
 *
 * Task:
 *   Create a sleek, polished bar renderer with:
 *     • Frame-rate–independent smoothing (tau-based EMA)
 *     • Uniform spacing and rounded corners
 *     • Subtle drop shadows
 *
 * Action:
 *   – Replace fixed alpha with dt-aware alpha = 1−exp(−dt/τ)
 *   – Introduce BAR_SPACING and compute bar width dynamically
 *   – Draw a semi-transparent shadow rectangle beneath each bar
 *   – Use DrawRectangleRounded() for smooth corners
 *
 * Response:
 *   Bars now flow fluidly, have breathing room and depth,
 *   and look “Appley” enough for macOS, iOS, or watchOS.
 */

#define VIS_BAR_COLOR   BLUE
#define SHADOW_COLOR    (Color){  0,   0,   0,  80}
static const float SMOOTH_TAU_SEC    = 0.06f;   // smoothing time constant (seconds)
static const float BAR_SPACING       = 2.0f;    // px between bars
static const float SHADOW_OFFSET_Y   = 4.0f;    // px drop for shadow
static const float SHADOW_HEIGHT     = 4.0f;    // px shadow thickness

typedef struct {
    size_t from, to;
} FreqBucket;

// Logarithmic-style frequency buckets (tweak these ranges as needed)
static FreqBucket buckets[] = {
    {  1,   2}, {  3,   4}, {  5,   6}, {  7,   9}, { 10,  13},
    { 14,  19}, { 20,  27}, { 28,  38}, { 39,  53}, { 54,  72},
    { 73,  97}, { 98, 129}, {130, 170}, {171, 223}, {224, 289},
    {290, 373}, {374, 479}, {480, 511}
};
#define BAR_COUNT (sizeof(buckets)/sizeof(buckets[0]))

static Rectangle *g_bar_recs   = NULL;
static float     *g_smooth     = NULL;
static int        g_win_w      = 0;
static int        g_win_h      = 0;
static float      g_max_height = 0.0f;
static float      g_prev_max   = 0.01f;  // for adaptive normalization

bool bars_init(size_t bin_count, int screen_w, int screen_h) {
    (void)bin_count;  // we derive bin count from buckets[]

    g_win_w      = screen_w;
    g_win_h      = screen_h;
    g_max_height = screen_h * 0.8f;  // leave 20% top margin

    g_bar_recs = calloc(BAR_COUNT, sizeof(Rectangle));
    g_smooth   = calloc(BAR_COUNT, sizeof(float));
    if (!g_bar_recs || !g_smooth) return false;

    // Compute bar width to fill screen with spacing
    float bar_width = (g_win_w - BAR_SPACING*(BAR_COUNT + 1)) / (float)BAR_COUNT;
    for (size_t i = 0; i < BAR_COUNT; ++i) {
        g_bar_recs[i].width  = bar_width;
        g_bar_recs[i].height = 0.0f;
        g_bar_recs[i].x      = BAR_SPACING + i * (bar_width + BAR_SPACING);
        g_bar_recs[i].y      = g_win_h;
        g_smooth[i]          = 0.0f;
    }
    return true;
}

void bars_render(const float *magnitudes) {
    if (!magnitudes || !g_bar_recs || !g_smooth) return;

    // Calculate frame-rate–aware smoothing factor
    float dt    = GetFrameTime();
    float alpha = 1.0f - expf(-dt / SMOOTH_TAU_SEC);

    for (size_t i = 0; i < BAR_COUNT; ++i) {
        // 1) Compute bucket RMS
        float sum = 0.0f;
        size_t count = buckets[i].to - buckets[i].from + 1;
        for (size_t j = buckets[i].from; j <= buckets[i].to; ++j) {
            sum += magnitudes[j] * magnitudes[j];
        }
        float rms = sqrtf(sum / (float)count);

        // 2) Adaptive max normalization
        if (rms > g_prev_max)
            g_prev_max = 0.8f * g_prev_max + 0.2f * rms;
        else
            g_prev_max = 0.98f * g_prev_max + 0.02f * rms;

        // 3) Normalize & perceptual remap
        float norm = fminf(rms / g_prev_max, 1.0f);
        norm = powf(norm, 0.6f);

        // 4) Per-band gain with edge damping
        float gain = 1.0f + 1.5f * ((float)i / (BAR_COUNT - 1));
        if (i == 0 || i == BAR_COUNT - 1) gain *= 0.5f;
        norm = fminf(norm * gain, 1.0f);

        // 5) Smooth value over time
        g_smooth[i] += alpha * (norm - g_smooth[i]);

        // 6) Compute rectangle
        float h = g_smooth[i] * g_max_height;
        Rectangle r = g_bar_recs[i];
        r.height = h;
        r.y      = g_win_h - h;

        // 7) Draw drop shadow
        Rectangle s = r;
        s.y     += SHADOW_OFFSET_Y;
        s.height = SHADOW_HEIGHT;
        DrawRectangleRounded(s, 0.15f, 4, SHADOW_COLOR);

        // 8) Draw rounded bar
        DrawRectangleRounded(r, 0.15f, 4, VIS_BAR_COLOR);
    }
}

void bars_shutdown(void) {
    free(g_bar_recs);
    free(g_smooth);
    g_bar_recs = NULL;
    g_smooth   = NULL;
}

