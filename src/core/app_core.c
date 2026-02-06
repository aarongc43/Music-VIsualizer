// src/core/app_core.c

#include "app_core.h"
#include "event_system.h"
#include "visualization_engine.h"
#include "audio_engine.h"
#include "fft/fft.h"
#include "colors.h"

#include <raylib.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_W  800
#define SCREEN_H  600
#define FFT_SIZE  (1 << 13)
#define RING_SECONDS 4

static Music g_music;

static float *g_time_buf = NULL;
static float *g_mag_buf  = NULL;

// Ring buffer holds float samples Audio thread writes & main thread reads
static float           *g_ring     = NULL;
static size_t           g_ring_cap  = 0; // capacity in float samples
static int              g_channels  = 0;
static atomic_size_t    g_ring_w    = 0; // write cursor samples

// Event handler: called when FFT has produced magnitudes
static void on_fft_ready(void *payload) {
    const float *mags = (const float *)payload;
    vis_update(mags, FFT_SIZE/2);
}

static void on_audio_samples(void *bufferData, unsigned int frames) {
    if (!g_ring || g_ring_cap == 0 || g_channels <= 0) return;

    const float *in = (const float *)bufferData;
    size_t samples = (size_t)frames * (size_t)g_channels;

    size_t w = atomic_load_explicit(&g_ring_w, memory_order_relaxed);

    for (size_t i = 0; i < samples; ++i) {
        g_ring[(w + i) % g_ring_cap] = in[i];
    }

    atomic_store_explicit(&g_ring_w, (w + samples) % g_ring_cap, memory_order_release);
}

static void read_latest_mono(float *out, size_t n) {
    if (!g_ring || g_ring_cap == 0 || g_channels <= 0) {
        for (size_t i = 0; i < n; ++i) out[i] = 0.0f;
        return;
    }

    size_t w = atomic_load_explicit(&g_ring_w, memory_order_acquire);

    // want the latest n frames of samples
    size_t back = n * (size_t)g_channels;
    size_t start = (w + g_ring_cap - (back % g_ring_cap)) % g_ring_cap;

    if (g_channels == 1) {
        for (size_t i = 0; i < n; ++i) {
            out[i] = g_ring[(start + i) % g_ring_cap];
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            float sum = 0.0f;
            size_t base = (start + i * (size_t)g_channels) % g_ring_cap;
            for (int ch = 0; ch < g_channels; ++ch) {
                sum += g_ring[(base + (size_t)ch) % g_ring_cap];
            }
            out[i] = sum / (float)g_channels;
        }
    }
}

void app_init(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

    g_music = LoadMusicStream("music/Rattlesnake.wav");
    if (!IsMusicValid(g_music)) {
        fprintf(stderr, "[app_core] ERROR: LoadMusicStream failed\n");
        exit(EXIT_FAILURE);
    }

    int sample_rate = (int)g_music.stream.sampleRate;
    g_channels = (int)g_music.stream.channels;

    if (sample_rate <= 0 || g_channels <= 0) {
        fprintf(stderr, "[app core] ERROR: invalid sample rate or channels\n");
        exit(EXIT_FAILURE);
    }

    // Allocate FFT buffers
    g_time_buf = (float *)malloc(sizeof(float) * FFT_SIZE);
    g_mag_buf  = (float *)malloc(sizeof(float) * FFT_SIZE/2 + 1);
    if (!g_time_buf || !g_mag_buf) {
        fprintf(stderr, "[app_core] ERROR: buffer allocation failed\n");
        exit(EXIT_FAILURE);
    }

    g_ring_cap = (size_t) RING_SECONDS * (size_t)sample_rate * (size_t)g_channels;
    g_ring = (float *)malloc(sizeof(float) * g_ring_cap);
    if(!g_ring) {
        fprintf(stderr, "[app core] ERROR: ring allocation failed\n");
        exit(EXIT_FAILURE);
    }
    atomic_store(&g_ring_w, 0);

    if (fft_init(FFT_SIZE) != 0) {
        fprintf(stderr, "[app_core] ERROR: fft_init failed\n");
        exit(EXIT_FAILURE);
    }

    if (!vis_init(SCREEN_W, SCREEN_H, FFT_SIZE)) {
        fprintf(stderr, "[app_core] ERROR: vis_init failed\n");
        exit(EXIT_FAILURE);
    }

    es_register(EVT_FFT_READY, on_fft_ready);

    // tap the audio stream
    AttachAudioStreamProcessor(g_music.stream, on_audio_samples);

    PlayMusicStream(g_music);
}

void app_run(void) {
    while (!WindowShouldClose()) {
        UpdateMusicStream(g_music);

        read_latest_mono(g_time_buf, FFT_SIZE);

        fft_compute(g_time_buf, g_mag_buf);
        es_emit(EVT_FFT_READY, g_mag_buf);

        BeginDrawing();
            ClearBackground(C_BLACK);
            vis_render();
        EndDrawing();
    }
}

void app_shutdown(void) {
    DetachAudioStreamProcessor(g_music.stream, on_audio_samples);

    vis_shutdown();
    fft_shutdown();

    free(g_ring);
    g_ring = NULL;
    g_ring_cap = 0;

    free(g_time_buf);
    g_time_buf = NULL;

    free(g_mag_buf);
    g_mag_buf = NULL;

    UnloadMusicStream(g_music);
    // Close audio & window
    CloseAudioDevice();
    CloseWindow();
}

int main(void) {
    app_init();
    app_run();
    app_shutdown();
    return 0;
}
