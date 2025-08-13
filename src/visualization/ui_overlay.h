// src/visualization/ui_overlay.h
#pragma once
#include <raylib.h>
#include <stdbool.h>

typedef struct {
    const char *title;   // song
    const char *artist;
    const char *album;
    float       length_s; // total secs
} TrackMeta;

typedef struct {
    // read-only outbound events (set by UI when user clicks)
    bool   want_prev, want_next, want_toggle_play;
    bool   want_seek;
    float  seek_target_s; // valid when want_seek
    // inputs you provide each frame
    bool   is_playing;
    float  cur_time_s;    // GetMusicTimePlayed()
    float  volume01;      // 0..1
} UiPlaybackIO;

bool ui_init(int screen_w, int screen_h, const char *font_path /*nullable*/);
void ui_shutdown(void);

// set once per track
void ui_set_track(const TrackMeta *m);

// set queue (<=10)
void ui_set_queue(const TrackMeta *items, int count, int cursor_index);

// per-frame: pass current state in, UI mutates *_OUT fields if user acts
void ui_update_and_render(UiPlaybackIO *io);
