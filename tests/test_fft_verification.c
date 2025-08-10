// tests/test_fft_verification.c

#include "vendor/unity/src/unity.h"
#include "vendor/unity/src/unity_internals.h"
#include "../src/audio/fft/fft.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef FS_HZ
#define FS_HZ 48000.0f     // set to your render sample rate
#endif

// Choose an FFT that matches your app (power of two)
#define NFFT 4096

static float *timebuf;
static float *magbuf;

static void gen_sine(float *x, size_t N, float f, float fs, float phase) {
    const float w = 2.0f * (float)M_PI * f / fs;
    for (size_t n = 0; n < N; ++n) x[n] = sinf(w * n + phase);
}

static void check_tone_at_bin(size_t k) {
    const float freq = (float)k * (float)FS_HZ / (float)NFFT; // coherent
    gen_sine(timebuf, NFFT, freq, FS_HZ, 0.37f);

    fft_compute(timebuf, magbuf);

    const size_t M = NFFT >> 1;
    // 1) peak bin near expected
    size_t argmax = 0; float vmax = 0.f;
    for (size_t i = 0; i < M; ++i) {
        if (magbuf[i] > vmax) { vmax = magbuf[i]; argmax = i; }
    }
    // Windowing may shift by at most 1 bin due to numeric noise; allow ±1
    TEST_ASSERT_INT_WITHIN(1, (int)k, (int)argmax);

    // 2) amplitude near 1.0 (Hann coherent gain corrected by 4/N)
    TEST_ASSERT_FLOAT_WITHIN(0.12f, 1.0f, vmax);

    // 3) energy concentrated within ±2 bins around the peak
    float total = 0.f, local = 0.f;
    for (size_t i = 0; i < M; ++i) total += magbuf[i] * magbuf[i];
    size_t lo = (argmax > 2) ? argmax - 2 : 0;
    size_t hi = (argmax + 2 < M) ? argmax + 2 : (M - 1);
    for (size_t i = lo; i <= hi; ++i) local += magbuf[i] * magbuf[i];
    TEST_ASSERT_TRUE( (local / (total + 1e-12f)) > 0.98f );
}

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(NFFT));
    timebuf = (float*)malloc(sizeof(float) * NFFT);
    magbuf  = (float*)malloc(sizeof(float) * (NFFT/2));
    TEST_ASSERT_NOT_NULL(timebuf);
    TEST_ASSERT_NOT_NULL(magbuf);
}
void tearDown(void) {
    free(timebuf); timebuf = NULL;
    free(magbuf);  magbuf  = NULL;
    fft_shutdown();
}

static void test_fft_log_sweep_100(void) {
    const size_t M = NFFT >> 1;
    const size_t k_min = (size_t)ceilf( 500.0f * (float)NFFT / (float)FS_HZ);
    const size_t k_max = (size_t)floorf(20000.0f * (float)NFFT / (float)FS_HZ);
    size_t steps = 100;

    size_t lo = (k_min < 1) ? 1 : k_min;
    size_t hi = (k_max >= M) ? (M - 1) : k_max;

    for (size_t s = 0; s < steps; ++s) {
        float t = (steps == 1) ? 0.f : (float)s / (float)(steps - 1);
        // log spacing in bin domain
        float kf = expf(logf((float)lo) * (1.f - t) + logf((float)hi) * t);
        size_t k = (size_t)llroundf(kf);
        if (k < 1) k = 1;
        if (k >= M) k = M - 1;
        check_tone_at_bin(k);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fft_log_sweep_100);
    return UNITY_END();
}

