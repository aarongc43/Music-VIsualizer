#ifndef UI_FONTS_H
#define UI_FONTS_H

#include <raylib.h>
#include <stdbool.h>

bool ui_fonts_init(void);
void ui_fonts_shutdown(void);

Font ui_font_body(void);    // oxanium
Font ui_font_display(void); // michroma

void ui_text_body(const char *text, int x, int y, float size, float spacing, Color color);
void ui_text_display(const char *text, int x, int y, float size, float spacing, Color color);

Vector2 ui_measure_body(const char *text, float size, float spacing);
Vector2 ui_measure_display(const char *text, float size, float spacing);

#endif
