#include "music_browser_ui.h"

#include "../core/colors.h"

#include <raylib.h>
#include <stddef.h>

#define LIB_PANEL_X 12
#define LIB_PANEL_Y 12
#define LIB_PANEL_W 300
#define LIB_PANEL_ROW_H 24
#define LIB_PANEL_HEADER_H 56
#define LIB_PANEL_FOOTER_H 28

void music_browser_ui_clamp_scroll(MusicLibrary *lib) {
    if (!lib) {
        return;
    }

    int panel_height = GetScreenHeight() - (LIB_PANEL_Y * 2);
    int list_height = panel_height - LIB_PANEL_HEADER_H - LIB_PANEL_FOOTER_H;
    if (list_height < LIB_PANEL_ROW_H) {
        list_height = LIB_PANEL_ROW_H;
    }

    size_t visible_rows = (size_t)(list_height / LIB_PANEL_ROW_H);
    if (visible_rows == 0) {
        visible_rows = 1;
    }

    if (lib->count == 0) {
        lib->scroll_offset = 0;
        lib->selected_index = 0;
        return;
    }

    if (lib->selected_index >= lib->count) {
        lib->selected_index = lib->count - 1;
    }

    if (lib->scroll_offset > lib->selected_index) {
        lib->scroll_offset = lib->selected_index;
    }

    if (lib->selected_index >= lib->scroll_offset + visible_rows) {
        lib->scroll_offset = lib->selected_index - visible_rows + 1;
    }
}

MusicBrowserUiAction music_browser_ui_handle_input(MusicLibrary *lib) {
    if (!lib) {
        return MUSIC_BROWSER_UI_ACTION_NONE;
    }

    if (IsKeyPressed(KEY_R)) {
        return MUSIC_BROWSER_UI_ACTION_RESCAN;
    }

    if (lib->count == 0) {
        return MUSIC_BROWSER_UI_ACTION_NONE;
    }

    if (IsKeyPressed(KEY_DOWN) && lib->selected_index + 1 < lib->count) {
        lib->selected_index++;
    }

    if (IsKeyPressed(KEY_UP) && lib->selected_index > 0) {
        lib->selected_index--;
    }

    if (IsKeyPressed(KEY_PAGE_DOWN)) {
        size_t step = 8;
        size_t next = lib->selected_index + step;
        lib->selected_index = (next < lib->count) ? next : (lib->count - 1);
    }

    if (IsKeyPressed(KEY_PAGE_UP)) {
        size_t step = 8;
        lib->selected_index = (lib->selected_index > step) ? (lib->selected_index - step) : 0;
    }

    float wheel = GetMouseWheelMove();
    if (wheel > 0.0f && lib->selected_index > 0) {
        lib->selected_index--;
    } else if (wheel < 0.0f && lib->selected_index + 1 < lib->count) {
        lib->selected_index++;
    }

    music_browser_ui_clamp_scroll(lib);

    if (IsKeyPressed(KEY_ENTER)) {
        return MUSIC_BROWSER_UI_ACTION_PLAY_SELECTED;
    }

    return MUSIC_BROWSER_UI_ACTION_NONE;
}

void music_browser_ui_draw(const MusicLibrary *lib, const char *status_text) {
    if (!lib) {
        return;
    }

    int screen_h = GetScreenHeight();
    int panel_h = screen_h - (LIB_PANEL_Y * 2);
    int list_y = LIB_PANEL_Y + LIB_PANEL_HEADER_H;
    int list_h = panel_h - LIB_PANEL_HEADER_H - LIB_PANEL_FOOTER_H;
    if (list_h < LIB_PANEL_ROW_H) {
        list_h = LIB_PANEL_ROW_H;
    }

    int visible_rows = list_h / LIB_PANEL_ROW_H;
    if (visible_rows < 1) {
        visible_rows = 1;
    }

    DrawRectangle(LIB_PANEL_X, LIB_PANEL_Y, LIB_PANEL_W, panel_h, C_WHITE);
    DrawRectangleLines(LIB_PANEL_X, LIB_PANEL_Y, LIB_PANEL_W, panel_h, C_YELLOW);

    DrawText("Music Library", LIB_PANEL_X + 12, LIB_PANEL_Y + 10, 20, C_WHITE);
    DrawText("R: rescan  Enter: play", LIB_PANEL_X + 12, LIB_PANEL_Y + 34, 12, (Color){190, 196, 203, 255});

    if (lib->count == 0) {
        DrawText("No tracks found in music/", LIB_PANEL_X + 12, list_y + 12, 16, C_WHITE);
    } else {
        size_t start = lib->scroll_offset;
        size_t end = start + (size_t)visible_rows;
        if (end > lib->count) {
            end = lib->count;
        }

        for (size_t i = start; i < end; ++i) {
            int y = list_y + (int)(i - start) * LIB_PANEL_ROW_H;
            bool is_selected = (i == lib->selected_index);
            bool is_playing = (i == lib->now_playing_index);

            if (is_selected) {
                DrawRectangle(LIB_PANEL_X + 6, y, LIB_PANEL_W - 12, LIB_PANEL_ROW_H - 2,
                              (Color){59, 90, 128, 255});
            }

            if (is_playing) {
                DrawText(">", LIB_PANEL_X + 10, y + 4, 16, C_YELLOW);
            }

            DrawText(lib->items[i].name ? lib->items[i].name : "(unnamed)",
                     LIB_PANEL_X + 28,
                     y + 4,
                     16,
                     C_WHITE);
        }
    }

    DrawText(status_text ? status_text : "", LIB_PANEL_X + 12, LIB_PANEL_Y + panel_h - 20, 12,
             (Color){202, 207, 213, 255});
}
