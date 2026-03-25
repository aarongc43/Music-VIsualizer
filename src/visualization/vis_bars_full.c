#include "vis_bars_full.h"
#include <raylib.h>
#include <stdlib.h>
#include <math.h>

#include "../core/colors.h"

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
// Nonlinear shaping applied after bin averaging.
// Higher values keep the meter calmer; lower values make quiet detail pop more.
static float g_gamma = 1.20f;

// Local thresholds so this module is self-contained
#define MIN_BAR_PX   1.0f
#define NOISE_GATE   0.0f        // input already smoothed; keep zero
#define SCANLINE_GAP_PX 6.0f
#define SCANLINE_THICKNESS_PX 1.0f

static Color color_lerp(Color a, Color b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    Color out;
    out.r = (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t);
    out.g = (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t);
    out.b = (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t);
    out.a = (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t);
    return out;
}

static Color color_alpha(Color c, unsigned char a)
{
    c.a = a;
    return c;
}

// Draw a solid sci-fi panel shape with two clipped corners.
// The cutout color is usually the background so the shape feels machined out.
static void draw_chamfered_fill(Rectangle r, float cut, Color fill, Color cutout)
{
    if (r.width <= 0.0f || r.height <= 0.0f) {
        return;
    }

    cut = fminf(cut, fminf(r.width, r.height) * 0.45f);
    DrawRectangleRec(r, fill);

    if (cut > 0.0f) {
        DrawTriangle((Vector2){ r.x, r.y },
                     (Vector2){ r.x + cut, r.y },
                     (Vector2){ r.x, r.y + cut },
                     cutout);

        DrawTriangle((Vector2){ r.x + r.width, r.y + r.height },
                     (Vector2){ r.x + r.width - cut, r.y + r.height },
                     (Vector2){ r.x + r.width, r.y + r.height - cut },
                     cutout);
    }
}

// Match the chamfered fill above with a thin technical outline.
static void draw_chamfered_outline(Rectangle r, float cut, float thick, Color color)
{
    if (r.width <= 0.0f || r.height <= 0.0f) {
        return;
    }

    cut = fminf(cut, fminf(r.width, r.height) * 0.45f);

    Vector2 p0 = { r.x + cut, r.y };
    Vector2 p1 = { r.x + r.width, r.y };
    Vector2 p2 = { r.x + r.width, r.y + r.height - cut };
    Vector2 p3 = { r.x + r.width - cut, r.y + r.height };
    Vector2 p4 = { r.x, r.y + r.height };
    Vector2 p5 = { r.x, r.y + cut };

    DrawLineEx(p0, p1, thick, color);
    DrawLineEx(p1, p2, thick, color);
    DrawLineEx(p2, p3, thick, color);
    DrawLineEx(p3, p4, thick, color);
    DrawLineEx(p4, p5, thick, color);
    DrawLineEx(p5, p0, thick, color);
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

    // Bars are mapped logarithmically so low frequencies get more visual space,
    // which usually feels more musical than uniform linear grouping.
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

    // Theme ramp: cool base -> acidic mid -> hot peak.
    const Color C_BASE = C_CYAN;
    const Color C_MID  = C_LIME;
    const Color C_PEAK = C_PINK;

    // All bars share the same vertical travel so they read like one instrument.
    const float rail_y = (float)g_height - g_hmax;

    for (size_t i = 0; i < g_bar_count; ++i) {
        const size_t s = g_bars[i].start_bin;
        const size_t e = g_bars[i].end_bin;

        // Average the already-normalized bins assigned to this bar.
        float sum = 0.0f;
        for (size_t b = s; b < e; ++b) sum += norm_bins_0_to_1[b];
        float avg = sum / (float)(e - s);

        // Apply a floor and perceptual shaping before converting to pixels.
        if (avg <= NOISE_GATE) avg = 0.0f;
        if (avg > 1.0f) avg = 1.0f;
        float shaped = powf(avg, g_gamma) * 1.2f;

        float H = shaped * g_hmax;
        if (H > g_hmax) H = g_hmax;
        if (H > 0.0f && H < MIN_BAR_PX) H = MIN_BAR_PX;

        Rectangle r = g_bars[i].rect;
        r.height = H;
        r.y      = (float)g_height - H;

        Rectangle rail = g_bars[i].rect;
        rail.y = rail_y;
        rail.height = g_hmax;

        // Back rail gives each bar a persistent slot even when the signal is quiet.
        float rail_cut = fminf(5.0f, rail.width * 0.26f);
        draw_chamfered_fill(rail, rail_cut, color_alpha(C_SURFACE_2, 120), C_BG);
        draw_chamfered_outline(rail, rail_cut, 1.0f, color_alpha(C_LINE_SOFT, 120));

        // Interpolate color by energy so the bar shifts hue as it gets louder.
        Color c = (shaped < 0.5f)
            ? color_lerp(C_BASE, C_MID, shaped * 2.0f)
            : color_lerp(C_MID, C_PEAK, (shaped - 0.5f) * 2.0f);

        // Chamfer amount is capped so very small bars do not collapse visually.
        float cut = fminf(6.0f, fminf(r.width, r.height) * 0.28f);

        if (H > 0.0f) {
            // Outer glow is a cheap fake bloom pass: slightly larger, lower alpha,
            // same geometry as the main bar.
            Rectangle glow = r;
            glow.x -= 1.5f;
            glow.width += 3.0f;
            glow.y -= 2.0f;
            glow.height += 2.0f;

            unsigned char glow_alpha = (unsigned char)(70.0f + shaped * 100.0f);
            draw_chamfered_fill(glow, cut + 1.0f, color_alpha(c, glow_alpha), C_BG);
        }

        draw_chamfered_fill(r, cut, c, C_BG);
        draw_chamfered_outline(r, cut, 1.0f, color_alpha(C_TEXT_PRIMARY, 38));

        if (H > 8.0f) {
            // Horizontal scanlines keep the bar feeling like a display element
            // rather than a flat painted rectangle.
            Color scan_color = color_lerp(c, C_TEXT_PRIMARY, 0.45f);
            unsigned char scan_alpha = (unsigned char)(28.0f + shaped * 56.0f);

            float y_start = r.y + 2.0f;
            float y_end = r.y + r.height - 1.0f;

            for (float y = y_start; y < y_end; y += SCANLINE_GAP_PX) {
                DrawLineEx((Vector2){ r.x + 1.0f, y },
                           (Vector2){ r.x + r.width - 1.0f, y },
                           SCANLINE_THICKNESS_PX,
                           color_alpha(scan_color, scan_alpha));
            }
        }

        if (H > 3.0f) {
            // Short top cap acts like a brighter read head near the current peak.
            Rectangle cap = {
                r.x,
                r.y,
                fmaxf(3.0f, r.width * 0.70f),
                fminf(2.0f, r.height)
            };
            Color cap_color = color_lerp(c, C_TEXT_PRIMARY, 0.35f);
            draw_chamfered_fill(cap, fminf(3.0f, cap.width * 0.24f), color_alpha(cap_color, 220), C_BG);
        }

        if (H > 10.0f) {
            // The inner spine adds one more technical detail line on taller bars.
            DrawLineEx((Vector2){ r.x + 2.0f, r.y + 3.0f },
                       (Vector2){ r.x + 2.0f, r.y + r.height - 3.0f },
                       1.0f,
                       color_alpha(C_TEXT_PRIMARY, 72));
        }
    }
}

void bars_full_shutdown(void)
{
    free(g_bars);
    g_bars = NULL;
    g_bar_count = 0;
    g_fft_bins = 0;
}
