// src/core/app_core.c

#include "app_core.h"
#include "event_system.h"
#include "visualization_engine.h"
#include "audio_engine.h"    // provides wav_load, wav_free, WAV type
#include "fft/fft.h"         // provides fft_init, fft_compute, fft_shutdown
#include <raylib.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define SCREEN_W  800
#define SCREEN_H  600
#define FFT_SIZE  (1 << 13)

static WAV    g_track;
static float *g_time_buf = NULL;  // raw samples → floats
static float *g_mag_buf  = NULL;  // magnitudes
static Music g_music;

// Event handler: called when FFT has produced magnitudes
static void on_fft_ready(void *payload) {
    const float *mags = (const float *)payload;
    vis_update(mags, FFT_SIZE/2);
}

void app_init(void) {
    // Window & audio
    InitWindow(SCREEN_W, SCREEN_H, "BragiBeats");
    InitAudioDevice();
    SetTargetFPS(60);

    // Load test WAV
    if (wav_load("music/love.wav", &g_track) != 0) {
        fprintf(stderr, "[app_core] ERROR: failed to load test WAV\n");
        exit(EXIT_FAILURE);
    }

    int chans = g_track.num_channels;
    printf("[INFO] WAV channels = %d\n", chans);

    g_music = LoadMusicStream("music/love.wav");
    PlayMusicStream(g_music);

    // Allocate FFT buffers
    g_time_buf = malloc(sizeof(float) * FFT_SIZE);
    g_mag_buf  = malloc(sizeof(float) * (FFT_SIZE));
    if (!g_time_buf || !g_mag_buf) {
        fprintf(stderr, "[app_core] ERROR: buffer allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize FFT subsystem
    if (fft_init(FFT_SIZE) != 0) {
        fprintf(stderr, "[app_core] ERROR: fft_init failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize visualization (bars + circles)
    if (!vis_init(SCREEN_W, SCREEN_H, FFT_SIZE)) {
        fprintf(stderr, "[app_core] ERROR: vis_init failed\n");
        exit(EXIT_FAILURE);
    }

    // Register FFT-ready callback
    es_register(EVT_FFT_READY, on_fft_ready);
}

void app_run(void) {
    while (!WindowShouldClose()) {
        UpdateMusicStream(g_music);

        float t = GetMusicTimePlayed(g_music);
        size_t sample_pos = (size_t)(t * g_track.sample_rate);

        // Fill time buffer from WAV (mono float)
        for (size_t i = 0; i < FFT_SIZE; ++i) {
            size_t frame = sample_pos + i;
            g_time_buf[i] = (frame < g_track.num_samples)
                          ? g_track.samples[frame]
                          : 0.0f;
        }

        // Compute FFT magnitudes and emit event
        fft_compute(g_time_buf, g_mag_buf);
        es_emit(EVT_FFT_READY, g_mag_buf);

        // Draw
        BeginDrawing();
            ClearBackground(BLACK);
            vis_render();
        EndDrawing();
    }
}

void app_shutdown(void) {
    // Tear down visualization & FFT
    vis_shutdown();
    fft_shutdown();

    // Free WAV and buffers
    wav_free(&g_track);
    free(g_time_buf);
    free(g_mag_buf);

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
