#ifndef MUSIC_LIBRARY_H
#define MUSIC_LIBRARY_H

#include <stddef.h>

typedef struct {
    char *path;
    char *name;
} TrackEntry;

typedef struct {
    TrackEntry *items;
    size_t count;
    size_t selected_index;
    size_t scroll_offset;
    size_t now_playing_index;
} MusicLibrary;

int music_library_scan(const char *dir, MusicLibrary *out_lib);
void music_library_free(MusicLibrary *lib);

#endif
