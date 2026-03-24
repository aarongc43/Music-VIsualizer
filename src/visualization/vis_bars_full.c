#include "vis_bars_full.h"
#include <raylib.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    Rectangle rect;
    size_t start_bin;
    size_t end_bin; // exclusive
} Bar;

static Bar   *g_bars = NULL;
static size_t g_bar_count = 0;
static size_t g_fft_bins = 0;

static int   g_width = 0, g_height = 0;
static float g_spacing = 0.0f;
static float g_bar_w = 0.0f;
static float g_hmax = 0.5f;      // bars reach at most half window height
// nonlinear scaling  curve applied to FFT magnitudes, which fixes perception
// gamma = 1.0 linear
// gamma > 1.0 boosts quieter bins, compresses loud ones
// gamme < 1.0 exaggerates loud peaks
static float g_gamma = 1.20f;

static float g_corner_px = 8.0f;
static int g_corner_segs = 10;

// Local thresholds so this module is self-contained
#define MIN_BAR_PX   1.0f
#define NOISE_GATE   0.0f        // input already smoothed; keep zero

static size_t clamp_size(size_t v, size_t lo, size_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool bars_full_init(size_t bin_count,
                    int screen_w,
                    int screen_h,
                    size_t bar_count,
                    float spacing_px,
                    float max_height_ratio)
{
    if (bin_count < 2 || screen_w <= 0 || screen_h <= 0 || bar_count < 1) return false;

    g_fft_bins  = bin_count;              // typically N/2 from your pipeline
    g_width         = screen_w;
    g_height         = screen_h;
    g_bar_count = bar_count;
    g_spacing   = spacing_px;
    g_hmax      = fmaxf(1.0f, max_height_ratio * (float)g_height);

    // geometry
    g_bar_w = ((float)g_width - (bar_count + 1) * g_spacing) / (float)bar_count;
    if (g_bar_w < 1.0f) g_bar_w = 1.0f;

    g_bars = (Bar*)malloc(sizeof(Bar) * bar_count);
    if (!g_bars) return false;

    const float lo = 1.0f;
    const float hi = (float)g_fft_bins;      // exclusive upper bound
    const float L  = logf(hi / lo);

    for (size_t i = 0; i < bar_count; ++i) {
        float a0 = (float)i       / (float)bar_count;
        float a1 = (float)(i + 1) / (float)bar_count;

        size_t s = (size_t)floorf(lo * expf(L * a0));
        size_t e = (size_t)floorf(lo * expf(L * a1));

        if (s < 1) s = 1;                  // skip DC bin
        if (e <= s) e = s + 1;             // ensure at least one bin
        if (e > g_fft_bins) e = g_fft_bins;

        g_bars[i].start_bin = s;
        g_bars[i].end_bin   = e;

        // precompute rect x/width; y/height filled per-frame
        g_bars[i].rect.x      = g_spacing + (float)i * (g_bar_w + g_spacing);
        g_bars[i].rect.width  = g_bar_w;
        g_bars[i].rect.y      = (float)g_height;
        g_bars[i].rect.height = 0.0f;
    }
    return true;
}

void bars_full_render(const float *norm_bins_0_to_1)
{
    if (!g_bars || !norm_bins_0_to_1) return;

    // Colors (cool blue gradient)
    const Color C_BASE = (Color){  10, 100, 200, 255 };
    const Color C_MID  = (Color){   0, 180, 255, 255 };
    const Color C_PEAK = (Color){ 110, 230, 255, 255 };

    for (size_t i = 0; i < g_bar_count; ++i) {
        const size_t s = g_bars[i].start_bin;
        const size_t e = g_bars[i].end_bin;

        // average the pre-normalized 0..1 magnitudes
        float sum = 0.0f;
        for (size_t b = s; b < e; ++b) sum += norm_bins_0_to_1[b];
        float avg = sum / (float)(e - s);

        // noise gate and perceptual curve
        if (avg <= NOISE_GATE) avg = 0.0f;
        if (avg > 1.0f) avg = 1.0f;
        float shaped = powf(avg, g_gamma) * 1.2f;

        float H = shaped * g_hmax;
        if (H > g_hmax) H = g_hmax;
        if (H > 0.0f && H < MIN_BAR_PX) H = MIN_BAR_PX;

        Rectangle r = g_bars[i].rect;
        r.height = H;
        r.y      = (float)g_height - H;

        // color interpolation base→mid→peak
        Color c;
        if (shaped < 0.5f) {
            float t = shaped * 2.0f;
            c.r = (unsigned char)(C_BASE.r + (C_MID.r - C_BASE.r) * t);
            c.g = (unsigned char)(C_BASE.g + (C_MID.g - C_BASE.g) * t);
            c.b = (unsigned char)(C_BASE.b + (C_MID.b - C_BASE.b) * t);
        } else {
            float t = (shaped - 0.5f) * 2.0f;
            c.r = (unsigned char)(C_MID.r + (C_PEAK.r - C_MID.r) * t);
            c.g = (unsigned char)(C_MID.g + (C_PEAK.g - C_MID.g) * t);
            c.b = (unsigned char)(C_MID.b + (C_PEAK.b - C_MID.b) * t);
        }
        c.a = 255;

        // --- rounded corners ---
        // Clamp corner radius to the current bar size to avoid artifacts
        float min_side   = fmaxf(1.0f, fminf(r.width, r.height));
        float roundness  = fminf(1.0f, g_corner_px / min_side);

        DrawRectangleRounded(r, roundness, g_corner_segs, c);
    }
}

void bars_full_shutdown(void)
{
    free(g_bars);
    g_bars = NULL;
    g_bar_count = 0;
    g_fft_bins = 0;
}
