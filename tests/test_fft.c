// tests/test_fft.c
#include "vendor/unity/src/unity.h"
#include "../src/audio/fft/fft.h"
#include <math.h>
#include <string.h>

#define N               1024
#define PI              3.14159265358979323846f
#define OFFBIN_TOL      1e-3f
#define PEAK_TOL        100.0f

static float time_buf[N];
static float mag_buf[N/2];

void setUp(void) {
    fft_shutdown();
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(N));
}

void tearDown(void) {
    fft_shutdown();
}

void test_zero_input(void) {
    memset(time_buf, 0, sizeof(time_buf));
    fft_compute(time_buf, mag_buf);
    for (size_t k = 0; k < N/2; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(OFFBIN_TOL, 0.0f, mag_buf[k]);
    }
}

void test_impulse(void) {
    memset(time_buf, 0, sizeof(time_buf));
    time_buf[N/2] = 1.0f;
    fft_compute(time_buf, mag_buf);
    float expected = mag_buf[0];
    for (size_t k = 0; k < N/2; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(OFFBIN_TOL, expected, mag_buf[k]);
    }
}

void test_pure_sine_bin5(void) {
    const size_t bin = 5;
    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = sinf(2*PI*bin*(float)i/(float)N);
    }
    fft_compute_raw(time_buf, mag_buf);
    for (size_t k = 0; k < N/2; ++k) {
        if (k == bin) {
            TEST_ASSERT_GREATER_THAN(PEAK_TOL, mag_buf[k]);
        } else {
            TEST_ASSERT_FLOAT_WITHIN(OFFBIN_TOL, 0.0f, mag_buf[k]);
        }
    }
}

void test_pure_cosine_bin10(void) {
    const size_t bin = 10;
    for (size_t i = 0; i < N; ++i) {
        time_buf[i] = cosf(2*PI*bin*(float)i/(float)N);
    }
    fft_compute_raw(time_buf, mag_buf);
    for (size_t k = 0; k < N/2; ++k) {
        if (k == bin) {
            TEST_ASSERT_GREATER_THAN(PEAK_TOL, mag_buf[k]);
        } else {
            TEST_ASSERT_FLOAT_WITHIN(OFFBIN_TOL, 0.0f, mag_buf[k]);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_input);
    RUN_TEST(test_impulse);
    RUN_TEST(test_pure_sine_bin5);
    RUN_TEST(test_pure_cosine_bin10);
    return UNITY_END();
}

