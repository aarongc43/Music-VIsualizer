// src/core/app_core.c

#include "app_core.h"

#include "../audio/fft/fft.h"
#include "../visualization/visualization_engine.h"
#include "colors.h"
#include "event_system.h"

#include <raylib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_W 800
#define SCREEN_H 600
#define RING_SECONDS 4
#define MUSIC_PATH "music/Rattlesnake.wav"

#define FFT_MIN_SIZE 128u
#define FFT_MAX_SIZE 8192u
#define TARGET_FFT_WINDOW_SECONDS 0.17f

static Music g_music = {0};

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
static int load_music_and_validate_stream(const char *path, int *out_sample_rate, int *out_channels);
static int alloc_processing_buffers(size_t fft_size, int sample_rate, int channels);
static int init_fft_and_visualization(size_t fft_size, int screen_w, int screen_h);
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

static int load_music_and_validate_stream(const char *path, int *out_sample_rate, int *out_channels) {
    g_music = LoadMusicStream(path);
    if (!IsMusicValid(g_music)) {
        fprintf(stderr, "[app_core] ERROR: LoadMusicStream failed\n");
        return -1;
    }

    g_music_loaded = true;

    *out_sample_rate = (int)g_music.stream.sampleRate;
    *out_channels = (int)g_music.stream.channels;
    if (*out_sample_rate <= 0 || *out_channels <= 0) {
        fprintf(stderr, "[app_core] ERROR: invalid sample rate or channels\n");
        return -1;
    }

    g_channels = *out_channels;
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
    int sample_rate = 0;
    int channels = 0;

    if (init_window_and_audio() != 0) {
        goto fail;
    }

    if (load_music_and_validate_stream(MUSIC_PATH, &sample_rate, &channels) != 0) {
        goto fail;
    }

    g_fft_size = choose_fft_size_for_sample_rate(sample_rate);
    fprintf(stdout,
            "[app_core] sample_rate=%d Hz, fft_size=%zu, bin_hz=%.2f\n",
            sample_rate,
            g_fft_size,
            (float)sample_rate / (float)g_fft_size);

    if (alloc_processing_buffers(g_fft_size, sample_rate, channels) != 0) {
        goto fail;
    }

    if (init_fft_and_visualization(g_fft_size, SCREEN_W, SCREEN_H) != 0) {
        goto fail;
    }

    attach_audio_processor();
    PlayMusicStream(g_music);
    return;

fail:
    app_shutdown();
    exit(EXIT_FAILURE);
}

void app_run(void) {
    while (!WindowShouldClose()) {
        UpdateMusicStream(g_music);

        read_latest_mono(g_time_buf, g_fft_size);
        fft_compute(g_time_buf, g_mag_buf);
        es_emit(EVT_FFT_READY, g_mag_buf);

        BeginDrawing();
        ClearBackground(C_BLACK);
        vis_render();
        EndDrawing();
    }
}

void app_shutdown(void) {
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

    if (g_music_loaded) {
        UnloadMusicStream(g_music);
        g_music = (Music){0};
        g_music_loaded = false;
    }

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
