#ifndef MUSIC_BROWSER_UI_H
#define MUSIC_BROWSER_UI_H

#include "../audio/music_library.h"

typedef enum {
    MUSIC_BROWSER_UI_ACTION_NONE = 0,
    MUSIC_BROWSER_UI_ACTION_PLAY_SELECTED,
    MUSIC_BROWSER_UI_ACTION_RESCAN
} MusicBrowserUiAction;

void music_browser_ui_clamp_scroll(MusicLibrary *lib);
MusicBrowserUiAction music_browser_ui_handle_input(MusicLibrary *lib);
void music_browser_ui_draw(const MusicLibrary *lib, const char *status_text);

#endif
