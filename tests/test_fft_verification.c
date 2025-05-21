// tests/test_fft_verification.c

#include "../src/audio/fft/fft.h"
#include "vendor/unity/src/unity.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FFT_SIZE 1024
#define PI 3.14159265358979323846f
#define EPSILON 0.01f

static float *time_buf = NULL;
static float *mag_buf  = NULL;

void setUp(void) {
    fft_shutdown();
    time_buf = malloc(sizeof(float) * TEST_FFT_SIZE);
    mag_buf  = malloc(sizeof(float) * (TEST_FFT_SIZE / 2));
    TEST_ASSERT_NOT_NULL(time_buf);
    TEST_ASSERT_NOT_NULL(mag_buf);
    memset(time_buf, 0, sizeof(float) * TEST_FFT_SIZE);
    memset(mag_buf,  0, sizeof(float) * (TEST_FFT_SIZE / 2));
}

void tearDown(void) {
    fft_shutdown();
    free(time_buf);
    free(mag_buf);
    time_buf = NULL;
    mag_buf  = NULL;
}

void test_fft_init_valid_size(void) {
    int err = fft_init(TEST_FFT_SIZE);
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, err);
}

void test_fft_init_invalid_size(void) {
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_BAD_SIZE, fft_init(0));
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_BAD_SIZE, fft_init(123));
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_BAD_SIZE, fft_init(16384));
}

void test_fft_zero_input(void) {
    fft_init(TEST_FFT_SIZE);
    memset(time_buf, 0, sizeof(float) * TEST_FFT_SIZE);
    fft_compute(time_buf, mag_buf);

    for (size_t k = 0; k < TEST_FFT_SIZE / 2; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0f, mag_buf[k]);
    }
}

void test_fft_impulse(void) {
    fft_init(TEST_FFT_SIZE);
    memset(time_buf, 0, sizeof(float) * TEST_FFT_SIZE);
    time_buf[TEST_FFT_SIZE / 2] = 1.0f;
    fft_compute(time_buf, mag_buf);

    float expected = mag_buf[0];
    for (size_t k = 0; k < TEST_FFT_SIZE / 2; ++k) {
        TEST_ASSERT_FLOAT_WITHIN(EPSILON, expected, mag_buf[k]);
    }
}

void test_fft_sine_pure_bin5(void) {
    fft_init(TEST_FFT_SIZE);
    size_t bin = 5;

    for (size_t i = 0; i < TEST_FFT_SIZE; ++i) {
        time_buf[i] = sinf(2.0f * PI * bin * i / TEST_FFT_SIZE);
    }

    fft_compute_raw(time_buf, mag_buf);

    for (size_t k = 0; k < TEST_FFT_SIZE / 2; ++k) {
        if (k == bin) {
            TEST_ASSERT_GREATER_THAN(0.5f, mag_buf[k]);
        } else {
            TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0f, mag_buf[k]);
        }
    }
}

void test_fft_cosine_bin10(void) {
    fft_init(TEST_FFT_SIZE);
    size_t bin = 10;

    for (size_t i = 0; i < TEST_FFT_SIZE; ++i) {
        time_buf[i] = cosf(2.0f * PI * bin * i / TEST_FFT_SIZE);
    }

    fft_compute_raw(time_buf, mag_buf);

    for (size_t k = 0; k < TEST_FFT_SIZE / 2; ++k) {
        if (k == bin) {
            TEST_ASSERT_GREATER_THAN(0.5f, mag_buf[k]);
        } else {
            TEST_ASSERT_FLOAT_WITHIN(EPSILON, 0.0f, mag_buf[k]);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fft_init_valid_size);
    RUN_TEST(test_fft_init_invalid_size);
    RUN_TEST(test_fft_zero_input);
    RUN_TEST(test_fft_impulse);
    RUN_TEST(test_fft_sine_pure_bin5);
    RUN_TEST(test_fft_cosine_bin10);
    return UNITY_END();
}

