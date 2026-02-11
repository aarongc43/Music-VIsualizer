#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "music_library.h"

static bool has_wav_extension(const char *name) {
    if (!name) return false;
    size_t len = strlen(name);
    if (len < 4) return false;
    const char *ext = name + (len - 4);
    return  (tolower((unsigned char) ext[0]) == '.') &&
            (tolower((unsigned char) ext[1]) == 'w') &&
            (tolower((unsigned char) ext[2]) == 'a') &&
            (tolower((unsigned char) ext[3]) == 'v');
}

static int track_entry_cmp(const void *a, const void *b) {
    const TrackEntry *ta = (const TrackEntry *)a;
    const TrackEntry *tb = (const TrackEntry *)b;

    int by_name = strcasecmp(ta->name, tb->name);
    if (by_name != 0) return by_name;

    return strcmp(ta->path, tb->path);
}

static char *dup_cstr(const char *src) {
    if (!src) {
        return NULL;
    }

    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, src, len + 1);
    return dst;
}

static void free_entries(TrackEntry *items, size_t count) {
    if (!items) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        free(items[i].path);
        free(items[i].name);
    }
    free(items);
}

static int push_track(TrackEntry **items,
                      size_t *count,
                      size_t *cap,
                      const char *full_path,
                      const char *name) {
    if (!items || !count || !cap || !full_path || !name) {
        return -1;
    }

    if (*count == *cap) {
        size_t new_cap = (*cap == 0u) ? 8u : (*cap * 2u);
        TrackEntry *new_items = realloc(*items, new_cap * sizeof(*new_items));
        if (!new_items) {
            return -1;
        }
        *items = new_items;
        *cap = new_cap;
    }

    char *path_copy = dup_cstr(full_path);
    char *name_copy = dup_cstr(name);
    if (!path_copy || !name_copy) {
        free(path_copy);
        free(name_copy);
        return -1;
    }

    (*items)[*count].path = path_copy;
    (*items)[*count].name = name_copy;
    (*count)++;
    return 0;
}

int music_library_scan(const char *dir_path, MusicLibrary *out_lib) {
    if (!dir_path || !out_lib) {
        fprintf(stderr, "[music_library] ERROR: invalid arguments\n");
        return -1;
    }

    if (dir_path[0] == '\0') {
        fprintf(stderr, "[music_library] ERROR: dir_path is empty\n");
        return -1;
    }

    memset(out_lib, 0, sizeof(*out_lib));
    out_lib->now_playing_index = SIZE_MAX;

    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "[music_library] ERROR: opendir failed for '%s'\n", dir_path);
        return -1;
    }

    TrackEntry *items = NULL;
    size_t count = 0, cap = 0;

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        if (!has_wav_extension(name)) {
            continue;
        }

        int needed = snprintf(NULL, 0, "%s/%s", dir_path, name);
        if (needed < 0) {
            fprintf(stderr, "[music_library] ERROR: failed to size full path for '%s'\n", name);
            closedir(dir);
            free_entries(items, count);
            return -1;
        }

        char *full_path = malloc((size_t)needed + 1u);
        if (!full_path) {
            fprintf(stderr, "[music_library] ERROR: out of memory creating path\n");
            closedir(dir);
            free_entries(items, count);
            return -1;
        }

        int written = snprintf(full_path, (size_t)needed + 1u, "%s/%s", dir_path, name);
        if (written != needed) {
            fprintf(stderr, "[music_library] ERROR: failed to write full path for '%s'\n", name);
            free(full_path);
            closedir(dir);
            free_entries(items, count);
            return -1;
        }

        if (push_track(&items, &count, &cap, full_path, name) != 0) {
            fprintf(stderr, "[music_library] ERROR: out of memory adding '%s'\n", name);
            free(full_path);
            closedir(dir);
            free_entries(items, count);
            return -1;
        }

        free(full_path);
    }

    closedir(dir);

    if (count > 1u) {
        qsort(items, count, sizeof(*items), track_entry_cmp);
    }

    out_lib->items = items;
    out_lib->count = count;
    out_lib->selected_index = 0u;
    out_lib->scroll_offset = 0u;
    out_lib->now_playing_index = SIZE_MAX;

    return 0;
}

void music_library_free(MusicLibrary *lib) {
    if (!lib) return;
    for (size_t i = 0; i < lib->count; ++i) {
        free(lib->items[i].path);
        free(lib->items[i].name);
    }

    free(lib->items);
    lib->items = NULL;
    lib->count = 0;
    lib->selected_index = 0;
    lib->scroll_offset = 0;
    lib->now_playing_index = SIZE_MAX;
}
