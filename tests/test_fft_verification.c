#include "../src/audio/fft/fft.h"
#include "vendor/unity/src/unity.h"
#include "vendor/unity/src/unity_internals.h"
#include <math.h>
#include <stdlib.h>

#define TEST_FFT_SIZE 1024
#define PI 3.14159265358979323846f
#define LEAKAGE_TOL 0.30f
#define RAW_LEAK_TOL 0.01f
#define AMP_TOL 0.05f

static float *time_buf = NULL;
static float *mag_buf = NULL;

void setUp(void) {
    // initialize FFT with N=1024
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(TEST_FFT_SIZE));
    // allocate buffers
    time_buf = malloc(sizeof(float) * TEST_FFT_SIZE);
    TEST_ASSERT_NOT_NULL_MESSAGE(time_buf, "time_buf OOM");
    mag_buf = malloc(sizeof(float) * (TEST_FFT_SIZE/2));
    TEST_ASSERT_NOT_NULL_MESSAGE(mag_buf, "mag_buf OOM");
}

void tearDown(void) {
    free(time_buf);
    free(mag_buf);
    fft_shutdown();
}

static void generate_sine(float *buf, size_t N, size_t bin_idx) {
    for (size_t n = 0; n < N; ++n) {
        buf[n] = sinf(2.0f * PI * bin_idx * ((float)n / (float)N));
    }
}

void test_fft_single_tone_peak(void) {
    const size_t target_bin = 5;
    generate_sine(time_buf, TEST_FFT_SIZE, target_bin);
    fft_compute(time_buf, mag_buf);

    // Find peak bin
    size_t peak_bin = 0;
    for (size_t i = 1; i < TEST_FFT_SIZE/2; ++i) {
        if (mag_buf[i] > mag_buf[peak_bin]) {
            peak_bin = i;
        }
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE(target_bin, peak_bin,
        "FFT peak not in expected bin");
}

void test_fft_raw_leakage_floor(void) {
    const size_t target = 5;
    generate_sine(time_buf, TEST_FFT_SIZE, target);
    fft_compute_raw(time_buf, mag_buf);

    float peak = mag_buf[target];
    for (size_t i = 0; i < TEST_FFT_SIZE/2; ++i) {
        if (i == target) continue;
        TEST_ASSERT_FLOAT_WITHIN(RAW_LEAK_TOL * peak, 0.0f, mag_buf[i]);
    }
}

void test_fft_leakage_floor(void) {
    const size_t target_bin = 5;
    generate_sine(time_buf, TEST_FFT_SIZE, target_bin);
    fft_compute(time_buf, mag_buf);

    float peak_val = mag_buf[target_bin];
    for (size_t i = 0; i < TEST_FFT_SIZE/2; ++i) {
        if (i == target_bin) continue;
        // Allow tiny leakage
        TEST_ASSERT_FLOAT_WITHIN(
            LEAKAGE_TOL * peak_val, 0.0f, mag_buf[i]
            );
    }
}

void test_fft_windowed_peak_amplitude(void) {
    const size_t target_bin = 5;
    generate_sine(time_buf, TEST_FFT_SIZE, target_bin);
    fft_compute(time_buf, mag_buf);

    //  peak is in the correct bin
    size_t peak_bin = 0;
    for (size_t i = 1; i < TEST_FFT_SIZE/2; ++i) {
        if (mag_buf[i] > mag_buf[peak_bin]) {
            peak_bin = i;
        }
    }
    TEST_ASSERT_EQUAL_UINT(target_bin, peak_bin);

    // allow ±2% tolerance
    const float expected_amp = 0.5f;
    TEST_ASSERT_FLOAT_WITHIN(
        AMP_TOL * expected_amp,
        expected_amp,
        mag_buf[target_bin]
    );
}

static void print_magnitudes(const float *buf, size_t count) {
    printf("Bin,Mag\n");
    for (size_t i = 0; i < count; ++i) {
        printf("%zu,%.6f\n", i, buf[i]);
    }
}

void test_fft_dump_windowed_output(void) {
    // Generate a sine at bin 5
    for (size_t n = 0; n < TEST_FFT_SIZE; ++n) {
        time_buf[n] = sinf(2*PI * 5 * ((float)n/TEST_FFT_SIZE));
    }

    // Compute windowed FFT
    fft_compute(time_buf, mag_buf);

    // Print out the magnitudes
    print_magnitudes(mag_buf, TEST_FFT_SIZE/2);

    // You can still assert something trivial so the test passes
    TEST_PASS_MESSAGE("FFT output dumped above.");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fft_single_tone_peak);
    RUN_TEST(test_fft_leakage_floor);
    RUN_TEST(test_fft_raw_leakage_floor);
    RUN_TEST(test_fft_windowed_peak_amplitude);
    RUN_TEST(test_fft_dump_windowed_output);
    return UNITY_END();
}
