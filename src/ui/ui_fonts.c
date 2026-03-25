#include <stddef.h>
#include "ui_fonts.h"

static Font g_font_body = {0};     // Oxanium
static Font g_font_display = {0};  // Michroma
static bool g_fonts_loaded = false;

bool ui_fonts_init(void) {
    if (g_fonts_loaded) {
        return true;
    }

    g_font_body = LoadFontEx("assets/fonts/oxanium.ttf", 64, NULL, 0);
    g_font_display = LoadFontEx("assets/fonts/michroma.ttf", 64, NULL, 0);

    if (g_font_body.texture.id == 0 || g_font_display.texture.id == 0) {
        if (g_font_body.texture.id != 0) UnloadFont(g_font_body);
        if (g_font_display.texture.id != 0) UnloadFont(g_font_display);
        g_font_body = (Font){0};
        g_font_display = (Font){0};
        return false;
    }

    SetTextureFilter(g_font_body.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(g_font_display.texture, TEXTURE_FILTER_BILINEAR);

    g_fonts_loaded = true;
    return true;
}

void ui_fonts_shutdown(void) {
    if (!g_fonts_loaded) {
        return;
    }

    UnloadFont(g_font_body);
    UnloadFont(g_font_display);

    g_font_body = (Font){0};
    g_font_display = (Font){0};
    g_fonts_loaded = false;
}

Font ui_font_body(void) {
    return g_font_body;
}

Font ui_font_display(void) {
    return g_font_display;
}

void ui_text_body(const char *text, int x, int y, float size, float spacing, Color color) {
    DrawTextEx(g_font_body, text, (Vector2){ (float)x, (float)y }, size, spacing, color);
}

void ui_text_display(const char *text, int x, int y, float size, float spacing, Color color) {
    DrawTextEx(g_font_display, text, (Vector2){ (float)x, (float)y }, size, spacing, color);
}

Vector2 ui_measure_body(const char *text, float size, float spacing) {
    return MeasureTextEx(g_font_body, text, size, spacing);
}

Vector2 ui_measure_display(const char *text, float size, float spacing) {
    return MeasureTextEx(g_font_display, text, size, spacing);
}
