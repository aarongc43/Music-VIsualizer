// tests/test_fft.c
#include "vendor/unity/src/unity.h"
#include "../src/audio/fft/fft.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define N 1024u
#define OUT_BINS (N / 2u + 1u)
#define PI 3.14159265358979323846f

static float time_buf[N];
static float mag_buf[OUT_BINS];

static void init_default_fft(void) {
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(N));
}

static size_t argmax(const float *x, size_t len) {
    size_t best_i = 0;
    float best_v = x[0];
    for (size_t i = 1; i < len; ++i) {
        if (x[i] > best_v) {
            best_v = x[i];
            best_i = i;
        }
    }
    return best_i;
}

static float energy_outside_band(const float *x, size_t len, size_t center, size_t half_width) {
    float e = 0.0f;
    for (size_t i = 0; i < len; ++i) {
        size_t lo = (center > half_width) ? (center - half_width) : 0;
        size_t hi = center + half_width;
        if (i < lo || i > hi) {
            e += x[i] * x[i];
        }
    }
    return e;
}

void setUp(void) {
    fft_shutdown();
    memset(time_buf, 0, sizeof(time_buf));
    memset(mag_buf, 0, sizeof(mag_buf));
}

void tearDown(void) {
    fft_shutdown();
}

void test_init_rejects_invalid_sizes(void) {
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_BAD_SIZE, fft_init(64));
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_BAD_SIZE, fft_init(1000));
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_BAD_SIZE, fft_init(16384));
}

void test_init_twice_requires_shutdown(void) {
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(1024));
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_ALREADY_INIT, fft_init(1024));
}

void test_get_size_tracks_lifecycle(void) {
    TEST_ASSERT_EQUAL_UINT64(0u, (unsigned long long)fft_get_size());
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(2048));
    TEST_ASSERT_EQUAL_UINT64(2048u, (unsigned long long)fft_get_size());
    fft_shutdown();
    TEST_ASSERT_EQUAL_UINT64(0u, (unsigned long long)fft_get_size());
}

void test_zero_input_gives_zero_spectrum(void) {
    init_default_fft();
    fft_compute_raw(time_buf, mag_buf);
    for (size_t k = 0; k < OUT_BINS; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, mag_buf[k]);
    }
}

void test_impulse_is_flat_in_raw_fft(void) {
    init_default_fft();
    time_buf[N / 2u] = 1.0f;
    fft_compute_raw(time_buf, mag_buf);

    for (size_t k = 0; k < OUT_BINS; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, mag_buf[k]);
    }
}

void test_dc_signal_peaks_at_bin_zero(void) {
    init_default_fft();
    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = 1.0f;
    }

    fft_compute_raw(time_buf, mag_buf);

    TEST_ASSERT_FLOAT_WITHIN(0.05f * (float)N, (float)N, mag_buf[0]);
    for (size_t k = 1; k < OUT_BINS; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, mag_buf[k]);
    }
}

void test_nyquist_signal_peaks_at_n_over_2(void) {
    init_default_fft();
    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = (i % 2u == 0u) ? 1.0f : -1.0f;
    }

    fft_compute_raw(time_buf, mag_buf);

    TEST_ASSERT_FLOAT_WITHIN(0.05f * (float)N, (float)N, mag_buf[N / 2u]);
    for (size_t k = 0; k < N / 2u; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, mag_buf[k]);
    }
}

void test_coherent_sine_lands_on_expected_bin_raw(void) {
    init_default_fft();
    const size_t bin = 13;

    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = sinf(2.0f * PI * (float)bin * (float)i / (float)N);
    }

    fft_compute_raw(time_buf, mag_buf);
    size_t peak = argmax(mag_buf, OUT_BINS);
    printf("[fft] coherent sine raw: expected=%zu peak=%zu amp=%.6f\n", bin, peak, mag_buf[peak]);
    TEST_ASSERT_EQUAL_UINT64((unsigned long long)bin, (unsigned long long)peak);
}

void test_windowed_coherent_sine_has_near_unity_amplitude(void) {
    init_default_fft();
    const size_t bin = 21;

    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = sinf(2.0f * PI * (float)bin * (float)i / (float)N);
    }

    fft_compute(time_buf, mag_buf);
    size_t peak = argmax(mag_buf, OUT_BINS);
    printf("[fft] coherent sine windowed: expected=%zu peak=%zu amp=%.6f\n", bin, peak, mag_buf[peak]);

    TEST_ASSERT_INT_WITHIN(1, (int)bin, (int)peak);
    TEST_ASSERT_FLOAT_WITHIN(0.12f, 1.0f, mag_buf[peak]);
}

void test_hann_window_reduces_far_leakage_for_offbin_tone(void) {
    init_default_fft();

    const float offbin = 11.5f;
    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = sinf(2.0f * PI * offbin * (float)i / (float)N);
    }

    fft_compute_raw(time_buf, mag_buf);
    size_t peak_raw = argmax(mag_buf, OUT_BINS);
    float leak_raw = energy_outside_band(mag_buf, OUT_BINS, peak_raw, 3);

    fft_compute(time_buf, mag_buf);
    size_t peak_win = argmax(mag_buf, OUT_BINS);
    float leak_win = energy_outside_band(mag_buf, OUT_BINS, peak_win, 3);

    printf("[fft] offbin leakage raw=%.6e windowed=%.6e\n", leak_raw, leak_win);
    TEST_ASSERT_TRUE(leak_win < leak_raw);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_rejects_invalid_sizes);
    RUN_TEST(test_init_twice_requires_shutdown);
    RUN_TEST(test_get_size_tracks_lifecycle);
    RUN_TEST(test_zero_input_gives_zero_spectrum);
    RUN_TEST(test_impulse_is_flat_in_raw_fft);
    RUN_TEST(test_dc_signal_peaks_at_bin_zero);
    RUN_TEST(test_nyquist_signal_peaks_at_n_over_2);
    RUN_TEST(test_coherent_sine_lands_on_expected_bin_raw);
    RUN_TEST(test_windowed_coherent_sine_has_near_unity_amplitude);
    RUN_TEST(test_hann_window_reduces_far_leakage_for_offbin_tone);

    return UNITY_END();
}
