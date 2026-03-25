#include "music_browser_ui.h"

#include "../core/colors.h"
#include "ui_fonts.h"

#include <raylib.h>
#include <stddef.h>
#include <stdio.h>

// change these to move the panel on screen
#define LIB_PANEL_X 12
#define LIB_PANEL_Y 12

// change this to adjust panel width
#define LIB_PANEL_W 500

// change this to adjust row height, will affect how many rows are visible
#define LIB_PANEL_ROW_H 24

// change these to adjust head/footer height
#define LIB_PANEL_HEADER_H 56
#define LIB_PANEL_FOOTER_H 28

static void ui_corner_marks(Rectangle r, int inset, int arm, Color color) {
    int left   = (int)r.x + inset;
    int right  = (int)(r.x + r.width) - inset;
    int top    = (int)r.y + inset;
    int bottom = (int)(r.y + r.height) - inset;

    // top-left
    DrawLine(left - arm, top, left + arm, top, color);
    DrawLine(left, top - arm, left, top + arm, color);

    // top-right
    DrawLine(right - arm, top, right + arm, top, color);
    DrawLine(right, top - arm, right, top + arm, color);

    // bottom-left
    DrawLine(left - arm, bottom, left + arm, bottom, color);
    DrawLine(left, bottom - arm, left, bottom + arm, color);

    // bottom-right
    DrawLine(right - arm, bottom, right + arm, bottom, color);
    DrawLine(right, bottom - arm, right, bottom + arm, color);
}

void music_browser_ui_clamp_scroll(MusicLibrary *lib) {
    if (!lib) {
        return;
    }

    // panel height is based on screen height minus top/bottom margins
    int panel_height = GetScreenHeight() - (LIB_PANEL_Y * 2);

    // visible list height
    int list_height = panel_height - LIB_PANEL_HEADER_H - LIB_PANEL_FOOTER_H;

    // ensure at one row can fit
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

    // clamp selected index inside bounds
    if (lib->selected_index >= lib->count) {
        lib->selected_index = lib->count - 1;
    }

    // keep scroll offset aligned with selection
    if (lib->scroll_offset > lib->selected_index) {
        lib->scroll_offset = lib->selected_index;
    }

    // scroll down if selection moves below visible window
    if (lib->selected_index >= lib->scroll_offset + visible_rows) {
        lib->scroll_offset = lib->selected_index - visible_rows + 1;
    }
}

MusicBrowserUiAction music_browser_ui_handle_input(MusicLibrary *lib) {
    if (!lib) {
        return MUSIC_BROWSER_UI_ACTION_NONE;
    }

    // 'R' to rescan music directory
    if (IsKeyPressed(KEY_R)) {
        return MUSIC_BROWSER_UI_ACTION_RESCAN;
    }

    if (lib->count == 0) {
        return MUSIC_BROWSER_UI_ACTION_NONE;
    }

    // arrow key navigation
    if (IsKeyPressed(KEY_DOWN) && lib->selected_index + 1 < lib->count) {
        lib->selected_index++;
    }

    if (IsKeyPressed(KEY_UP) && lib->selected_index > 0) {
        lib->selected_index--;
    }

    // mouse wheel scroll
    float wheel = GetMouseWheelMove();
    if (wheel > 0.0f && lib->selected_index > 0) {
        lib->selected_index--;
    } else if (wheel < 0.0f && lib->selected_index + 1 < lib->count) {
        lib->selected_index++;
    }

    music_browser_ui_clamp_scroll(lib);

    // press enter to play selected track
    if (IsKeyPressed(KEY_ENTER)) {
        return MUSIC_BROWSER_UI_ACTION_PLAY_SELECTED;
    }

    return MUSIC_BROWSER_UI_ACTION_NONE;
}

void music_browser_ui_draw(const MusicLibrary *lib, const char *status_text) {
    if (!lib) {
        return;
    }

    /*
     * layout
     */
    const int TARGET_VISIBLE_ROWS   = 5;

    // panel spacing
    const int PANEL_PAD_TOP         = 12;
    const int PANEL_PAD_BOTTOM      = 12;
    const int PANEL_TEXT_X          = 18;

    // header spacing
    const int TITLE_FONT            = 16;
    const int HELP_FONT             = 10;
    const int TITLE_TO_HELP_GAP     = 10;
    const int HELP_TO_LIST_GAP      = 16;

    // list frame spacing
    const int LIST_FRAME_PAD_X      = 10;
    const int LIST_INSET_LEFT       = 18;
    const int LIST_INSET_RIGHT      = 18;
    const int LIST_INSET_TOP        = 14;
    const int LIST_INSET_BOTTOM     = 12;

    // row content alignment
    const int ROW_INDEX_X           = 18;
    const int ROW_INDEX_Y           =  5;
    const int ROW_PILL_X            = 66;
    const int ROW_PILL_Y            =  7;
    const int ROW_PILL_SIZE         = 10;
    const int ROW_TEXT_Y            =  4;
    const int ROW_GAP_AFTER_PILL    = 16;

    /*
     * header positions
     */
    const int title_x = LIB_PANEL_X + PANEL_TEXT_X;
    const int title_y = LIB_PANEL_Y + PANEL_PAD_TOP;

    const int help_x = title_x;
    const int help_y = title_y + TITLE_FONT + TITLE_TO_HELP_GAP;

    /*
     * list frame positions
     */
    const int list_y = help_y + HELP_FONT + HELP_TO_LIST_GAP;
    const int list_h =
        LIST_INSET_TOP +
        (TARGET_VISIBLE_ROWS * LIB_PANEL_ROW_H) +
        LIST_INSET_BOTTOM;
    const int panel_h = 
        (list_y - LIB_PANEL_Y) +
        list_h +
        PANEL_PAD_BOTTOM;
    const int visible_rows = TARGET_VISIBLE_ROWS;

    Rectangle panel = {
        LIB_PANEL_X,
        LIB_PANEL_Y,
        LIB_PANEL_W,
        panel_h
    };

    Rectangle list_rect = {
        LIB_PANEL_X + LIST_FRAME_PAD_X,
        list_y,
        LIB_PANEL_W - (LIST_FRAME_PAD_X * 2),
        list_h
    };

    Rectangle list_content = {
        list_rect.x + LIST_INSET_LEFT,
        list_rect.y + LIST_INSET_TOP,
        list_rect.width - LIST_INSET_LEFT - LIST_INSET_RIGHT,
        TARGET_VISIBLE_ROWS * LIB_PANEL_ROW_H
    };

    /*
     * outer shell
     */
    DrawRectangleRec(panel, C_SURFACE_1);
    DrawRectangleLinesEx(panel, 2, C_LINE_STRONG);

    /*
     * header
     */
    ui_text_display("Music Library", title_x, title_y, 18, 1.0f, C_TEXT_TERTIARY);
    ui_text_body("[R] Rescan        [ENTER] PLAY", help_x, help_y, 11, 1.0f, C_TEXT_PRIMARY);

    /*
     * list frame
     */
    DrawRectangleRec(list_rect, C_SURFACE_1);
    DrawRectangleLinesEx(list_rect, 2, C_LINE_SOFT);

    if (lib->count == 0) {
        DrawText("No tracks found in music/",
                 (int)list_content.x,
                 (int)list_content.y,
                 16,
                 C_TEXT_SECONDARY);
        ui_corner_marks(panel, 10, 3, C_LINE_SOFT);
        ui_corner_marks(list_rect, 10, 3, C_LINE_SOFT);
        return;
    }

    /*
     * song visible range
     */
    size_t start = lib->scroll_offset;
    size_t end = start + (size_t)visible_rows;
    if (end > lib->count) {
        end = lib->count;
    }

    /*
     * rows
     */
    for (size_t i = start; i < end; ++i) {
        int y = (int)list_content.y + (int)(i - start) * LIB_PANEL_ROW_H;
        bool is_selected = (i == lib->selected_index);
        bool is_playing = (i == lib->now_playing_index);

        char idx[8];
        snprintf(idx, sizeof(idx), "%02u", (unsigned)(i + 1));

        Rectangle row = {
            list_content.x,
            (float)y,
            list_content.width,
            LIB_PANEL_ROW_H
        };

        // base row
        DrawRectangleRec(row, C_SURFACE_2);
        DrawLine((int)row.x,
                 (int)(row.y + row.height - 1),
                 (int)(row.x + row.width),
                 (int)(row.y + row.height - 1),
                 C_LINE_SOFT);

        // selected row
        if (is_selected) {
            DrawRectangleRec(row, C_LIME);
            DrawLine((int)row.x, (int)row.y,
                     (int)(row.x + row.width), (int)row.y,
                     C_LINE_STRONG);
        }

        // now playing pill marker
        Rectangle pill = {
            row.x + ROW_PILL_X,
            row.y + ROW_PILL_Y,
            ROW_PILL_SIZE,
            ROW_PILL_SIZE
        };

        if (is_playing) {
            DrawRectangleRec(pill, is_selected ? C_TAG_TEXT : C_LIME);
            DrawRectangleLinesEx(pill, 1, is_selected ? C_TAG_TEXT : C_LINE_STRONG);
        }

        // row text colors
        Color row_idx_color  = is_selected ? C_TAG_TEXT : C_TEXT_TERTIARY;
        Color row_name_color = is_selected ? C_TAG_TEXT : C_TEXT_SECONDARY;

        // row text positions
        const int idx_x  = (int)row.x + ROW_INDEX_X;
        const int idx_y  = (int)row.y + ROW_INDEX_Y;
        const int name_x = (int)pill.x + ROW_PILL_SIZE + ROW_GAP_AFTER_PILL;
        const int name_y = (int)row.y + ROW_TEXT_Y;

        ui_text_body(idx, idx_x, idx_y, 14, 0.5f, row_idx_color);

        ui_text_body(lib->items[i].name ? lib->items[i].name : "(unamed)",
                     name_x, name_y, 16, 0.5f, row_name_color);
    }
    ui_corner_marks(panel, 10, 3, C_LINE_SOFT);
    ui_corner_marks(list_rect, 10, 3, C_LINE_SOFT);
}
