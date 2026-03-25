// src/core/app_core.c

#include "app_core.h"

#include "../audio/fft/fft.h"
#include "../audio/music_library.h"
#include "../ui/music_browser_ui.h"
#include "../visualization/visualization_engine.h"
#include "../ui/ui_fonts.h"
#include "colors.h"
#include "event_system.h"

#include <raylib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 1800
#define SCREEN_H 1013
#define RING_SECONDS 4

#define MUSIC_DIR_PATH "music"

#define FFT_MIN_SIZE 128u
#define FFT_MAX_SIZE 8192u
#define TARGET_FFT_WINDOW_SECONDS 0.17f

#define DEFAULT_SAMPLE_RATE 48000

static Music g_music = {0};
static MusicLibrary g_library = {0};
static char g_status_text[256] = "";

static float *g_time_buf = NULL;
static float *g_mag_buf = NULL;
static size_t g_fft_size = 0;

// Ring buffer holds float samples. Audio thread writes; main thread reads.
static float *g_ring = NULL;
static size_t g_ring_cap = 0; // capacity in float samples
static int g_channels = 0;
static atomic_size_t g_ring_w = 0; // write cursor in samples

static bool g_window_initialized = false;
static bool g_audio_initialized = false;
static bool g_music_loaded = false;
static bool g_fft_initialized = false;
static bool g_vis_initialized = false;
static bool g_audio_processor_attached = false;

static size_t next_pow2_size(size_t value);
static size_t choose_fft_size_for_sample_rate(int sample_rate);
static int init_window_and_audio(void);
static int alloc_processing_buffers(size_t fft_size, int sample_rate, int channels);
static int init_fft_and_visualization(size_t fft_size, int screen_w, int screen_h);
static void shutdown_processing_pipeline(void);
static int play_track_by_index(size_t idx);
static size_t find_library_index_by_path(const MusicLibrary *lib, const char *path);
static int rescan_music_library(void);
static void attach_audio_processor(void);
static void detach_audio_processor(void);
static void free_processing_buffers(void);

// Event handler: called when FFT has produced magnitudes.
static void on_fft_ready(void *payload) {
    const float *mags = (const float *)payload;
    vis_update(mags, g_fft_size / 2);
}

static size_t next_pow2_size(size_t value) {
    size_t n = 1;
    while (n < value) {
        n <<= 1;
    }
    return n;
}

static size_t choose_fft_size_for_sample_rate(int sample_rate) {
    if (sample_rate <= 0) {
        return FFT_MAX_SIZE;
    }

    size_t target = (size_t)((float)sample_rate * TARGET_FFT_WINDOW_SECONDS);
    if (target < FFT_MIN_SIZE) {
        target = FFT_MIN_SIZE;
    }

    size_t chosen = next_pow2_size(target);
    if (chosen < FFT_MIN_SIZE) {
        chosen = FFT_MIN_SIZE;
    }
    if (chosen > FFT_MAX_SIZE) {
        chosen = FFT_MAX_SIZE;
    }

    return chosen;
}

static void on_audio_samples(void *buffer_data, unsigned int frames) {
    if (!g_ring || g_ring_cap == 0 || g_channels <= 0) {
        return;
    }

    const float *in = (const float *)buffer_data;
    size_t samples = (size_t)frames * (size_t)g_channels;
    size_t w = atomic_load_explicit(&g_ring_w, memory_order_relaxed);

    for (size_t i = 0; i < samples; ++i) {
        g_ring[(w + i) % g_ring_cap] = in[i];
    }

    atomic_store_explicit(&g_ring_w, (w + samples) % g_ring_cap, memory_order_release);
}

static void read_latest_mono(float *out, size_t n) {
    if (!g_ring || g_ring_cap == 0 || g_channels <= 0) {
        for (size_t i = 0; i < n; ++i) {
            out[i] = 0.0f;
        }
        return;
    }

    size_t w = atomic_load_explicit(&g_ring_w, memory_order_acquire);
    size_t back = n * (size_t)g_channels;
    size_t start = (w + g_ring_cap - (back % g_ring_cap)) % g_ring_cap;

    if (g_channels == 1) {
        for (size_t i = 0; i < n; ++i) {
            out[i] = g_ring[(start + i) % g_ring_cap];
        }
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        float sum = 0.0f;
        size_t base = (start + i * (size_t)g_channels) % g_ring_cap;

        for (int ch = 0; ch < g_channels; ++ch) {
            sum += g_ring[(base + (size_t)ch) % g_ring_cap];
        }

        out[i] = sum / (float)g_channels;
    }
}

static int init_window_and_audio(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Visualizer");
    g_window_initialized = true;

    InitAudioDevice();
    g_audio_initialized = true;

    SetTargetFPS(60);
    return 0;
}

static void shutdown_processing_pipeline(void) {
    detach_audio_processor();
    es_reset();

    if (g_vis_initialized) {
        vis_shutdown();
        g_vis_initialized = false;
    }

    if (g_fft_initialized) {
        fft_shutdown();
        g_fft_initialized = false;
    }

    free_processing_buffers();
}

static size_t find_library_index_by_path(const MusicLibrary *lib, const char *path) {
    if (!lib || !path) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < lib->count; ++i) {
        if (lib->items[i].path && strcmp(lib->items[i].path, path) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

static int play_track_by_index(size_t idx) {
    if (idx >= g_library.count) {
        snprintf(g_status_text, sizeof(g_status_text), "Invalid track index");
        return -1;
    }

    const char *path = g_library.items[idx].path;
    if (!path) {
        snprintf(g_status_text, sizeof(g_status_text), "Track has invalid path");
        return -1;
    }

    Music new_music = LoadMusicStream(path);
    if (!IsMusicValid(new_music)) {
        snprintf(g_status_text, sizeof(g_status_text), "Failed to load: %s", g_library.items[idx].name);
        return -1;
    }

    int sample_rate = (int)new_music.stream.sampleRate;
    int channels = (int)new_music.stream.channels;
    if (sample_rate <= 0 || channels <= 0) {
        UnloadMusicStream(new_music);
        snprintf(g_status_text, sizeof(g_status_text), "Invalid stream format: %s", g_library.items[idx].name);
        return -1;
    }

    shutdown_processing_pipeline();

    if (g_music_loaded) {
        StopMusicStream(g_music);
        UnloadMusicStream(g_music);
        g_music = (Music){0};
        g_music_loaded = false;
    }

    g_music = new_music;
    g_music_loaded = true;
    g_channels = channels;
    g_fft_size = choose_fft_size_for_sample_rate(sample_rate);

    if (alloc_processing_buffers(g_fft_size, sample_rate, channels) != 0) {
        StopMusicStream(g_music);
        UnloadMusicStream(g_music);
        g_music = (Music){0};
        g_music_loaded = false;
        snprintf(g_status_text, sizeof(g_status_text), "Failed to allocate buffers");
        return -1;
    }

    if (init_fft_and_visualization(g_fft_size, GetScreenWidth(), GetScreenHeight()) != 0) {
        free_processing_buffers();
        StopMusicStream(g_music);
        UnloadMusicStream(g_music);
        g_music = (Music){0};
        g_music_loaded = false;
        snprintf(g_status_text, sizeof(g_status_text), "Failed to initialize FFT/visualization");
        return -1;
    }

    attach_audio_processor();
    PlayMusicStream(g_music);

    g_library.selected_index = idx;
    g_library.now_playing_index = idx;

    snprintf(g_status_text,
             sizeof(g_status_text),
             "Now playing: %s",
             g_library.items[idx].name ? g_library.items[idx].name : "(unknown)");

    return 0;
}

static int rescan_music_library(void) {
    MusicLibrary refreshed = {0};
    size_t selected_before = g_library.selected_index;
    size_t now_playing_before = g_library.now_playing_index;
    const char *selected_path = (selected_before < g_library.count) ? g_library.items[selected_before].path : NULL;
    const char *playing_path = (now_playing_before < g_library.count) ? g_library.items[now_playing_before].path : NULL;

    if (music_library_scan(MUSIC_DIR_PATH, &refreshed) != 0) {
        snprintf(g_status_text, sizeof(g_status_text), "Rescan failed");
        return -1;
    }

    if (selected_path) {
        size_t idx = find_library_index_by_path(&refreshed, selected_path);
        if (idx != SIZE_MAX) {
            refreshed.selected_index = idx;
        }
    }

    if (playing_path) {
        size_t idx = find_library_index_by_path(&refreshed, playing_path);
        refreshed.now_playing_index = idx;
    }

    music_library_free(&g_library);
    g_library = refreshed;
    music_browser_ui_clamp_scroll(&g_library);

    if (g_library.count == 0) {
        shutdown_processing_pipeline();
        if (g_music_loaded) {
            StopMusicStream(g_music);
            UnloadMusicStream(g_music);
            g_music = (Music){0};
            g_music_loaded = false;
        }

        g_fft_size = choose_fft_size_for_sample_rate(DEFAULT_SAMPLE_RATE);
        if (alloc_processing_buffers(g_fft_size, DEFAULT_SAMPLE_RATE, 1) == 0) {
            (void)init_fft_and_visualization(g_fft_size, GetScreenWidth(), GetScreenHeight());
        }

        snprintf(g_status_text, sizeof(g_status_text), "No tracks found in %s", MUSIC_DIR_PATH);
        return 0;
    }

    if (g_library.now_playing_index == SIZE_MAX) {
        snprintf(g_status_text, sizeof(g_status_text), "Rescanned: %zu tracks", g_library.count);
    } else {
        snprintf(g_status_text, sizeof(g_status_text), "Rescanned: now playing preserved");
    }

    return 0;
}

static int alloc_processing_buffers(size_t fft_size, int sample_rate, int channels) {
    g_time_buf = malloc(sizeof(float) * fft_size);
    g_mag_buf = malloc(sizeof(float) * (fft_size / 2 + 1));
    if (!g_time_buf || !g_mag_buf) {
        fprintf(stderr, "[app_core] ERROR: processing buffer allocation failed\n");
        free_processing_buffers();
        return -1;
    }

    g_ring_cap = (size_t)RING_SECONDS * (size_t)sample_rate * (size_t)channels;
    g_ring = malloc(sizeof(float) * g_ring_cap);
    if (!g_ring) {
        fprintf(stderr, "[app_core] ERROR: ring buffer allocation failed\n");
        free_processing_buffers();
        return -1;
    }

    atomic_store_explicit(&g_ring_w, 0, memory_order_relaxed);
    return 0;
}

static int init_fft_and_visualization(size_t fft_size, int screen_w, int screen_h) {
    if (fft_init(fft_size) != FFT_SUCCESS) {
        fprintf(stderr, "[app_core] ERROR: fft_init failed\n");
        return -1;
    }
    g_fft_initialized = true;

    if (!vis_init(screen_w, screen_h, fft_size)) {
        fprintf(stderr, "[app_core] ERROR: vis_init failed\n");
        fft_shutdown();
        g_fft_initialized = false;
        return -1;
    }
    g_vis_initialized = true;

    es_reset();
    es_register(EVT_FFT_READY, on_fft_ready);
    return 0;
}

static void attach_audio_processor(void) {
    if (g_music_loaded && !g_audio_processor_attached) {
        AttachAudioStreamProcessor(g_music.stream, on_audio_samples);
        g_audio_processor_attached = true;
    }
}

static void detach_audio_processor(void) {
    if (g_music_loaded && g_audio_processor_attached) {
        DetachAudioStreamProcessor(g_music.stream, on_audio_samples);
        g_audio_processor_attached = false;
    }
}

static void free_processing_buffers(void) {
    free(g_ring);
    g_ring = NULL;
    g_ring_cap = 0;

    free(g_time_buf);
    g_time_buf = NULL;

    free(g_mag_buf);
    g_mag_buf = NULL;

    g_fft_size = 0;
    g_channels = 0;
}

void app_init(void) {
    if (init_window_and_audio() != 0) {
        goto fail;
    }

    if (!ui_fonts_init()) {
        fprintf(stderr, "[app_core] ERROR: ui_fonts_init failed\n");
        goto fail;
    }

    if (music_library_scan(MUSIC_DIR_PATH, &g_library) != 0) {
        snprintf(g_status_text, sizeof(g_status_text), "Failed to scan %s", MUSIC_DIR_PATH);
        goto fail;
    }

    if (g_library.count > 0) {
        if (play_track_by_index(0) != 0) {
            goto fail;
        }
    } else {
        g_fft_size = choose_fft_size_for_sample_rate(DEFAULT_SAMPLE_RATE);
        if (alloc_processing_buffers(g_fft_size, DEFAULT_SAMPLE_RATE, 1) != 0) {
            goto fail;
        }

        if (init_fft_and_visualization(g_fft_size, SCREEN_W, SCREEN_H) != 0) {
            goto fail;
        }

        snprintf(g_status_text, sizeof(g_status_text), "No tracks found in %s", MUSIC_DIR_PATH);
    }

    return;

fail:
    app_shutdown();
    exit(EXIT_FAILURE);
}

void app_run(void) {
    while (!WindowShouldClose()) {
        if (g_music_loaded) {
            UpdateMusicStream(g_music);
        }

        MusicBrowserUiAction ui_action = music_browser_ui_handle_input(&g_library);
        if (ui_action == MUSIC_BROWSER_UI_ACTION_RESCAN) {
            (void)rescan_music_library();
        } else if (ui_action == MUSIC_BROWSER_UI_ACTION_PLAY_SELECTED && g_library.count > 0) {
            (void)play_track_by_index(g_library.selected_index);
        }

        read_latest_mono(g_time_buf, g_fft_size);
        fft_compute(g_time_buf, g_mag_buf);
        es_emit(EVT_FFT_READY, g_mag_buf);

        BeginDrawing();
        ClearBackground(C_BG);
        vis_render();
        music_browser_ui_draw(&g_library, g_status_text);
        EndDrawing();
    }
}

void app_shutdown(void) {
    shutdown_processing_pipeline();

    if (g_music_loaded) {
        StopMusicStream(g_music);
        UnloadMusicStream(g_music);
        g_music = (Music){0};
        g_music_loaded = false;
    }

    music_library_free(&g_library);

    ui_fonts_shutdown();

    if (g_audio_initialized) {
        CloseAudioDevice();
        g_audio_initialized = false;
    }

    if (g_window_initialized) {
        CloseWindow();
        g_window_initialized = false;
    }
}

int main(void) {
    app_init();
    app_run();
    app_shutdown();
    return 0;
}
