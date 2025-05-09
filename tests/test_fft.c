#include "vendor/unity/src/unity.h"
#include "../src/audio/fft/fft.h"
#include <math.h>
#include <stddef.h>

void setUp(void) {
    fft_shutdown();
}

void tearDown(void) {
    fft_shutdown();
}

void test_fft_init_valid_size(void) {
    TEST_ASSERT_EQUAL_INT(0, fft_init(256));
    TEST_ASSERT_EQUAL_size_t(256, fft_get_size());
}

void test_fft_init_invalid_size(void) {
    TEST_ASSERT_NOT_EQUAL(0, fft_init(200));
    TEST_ASSERT_EQUAL_size_t(0, fft_get_size());
}

void test_fft_double_init(void) {
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(256));
    TEST_ASSERT_EQUAL_INT(FFT_ERROR_ALREADY_INIT, fft_init(256));
    fft_shutdown();
    // after shutdown it works again
    TEST_ASSERT_EQUAL_INT(FFT_SUCCESS, fft_init(256));
    fft_shutdown();
}

void test_fft_sine_pure_bin5(void) {
    const size_t N = 256;
    const size_t k = 5;
    float time_data[N];
    float out_mag[N/2];

    // initialize FFT
    TEST_ASSERT_EQUAL_INT(0, fft_init(N));

    // build sine wave at bin k
    for (size_t n = 0; n < N; ++n) {
        time_data[n] = sinf(2.0f * M_PI * (float)k * (float)n / (float)N);
    }

    // compute FFT magnitudes
    fft_compute(time_data, out_mag);

    // find the index of maximum magnitude
    size_t max_i = 0;
    float max_v = 0.0f;
    for (size_t i = 0; i < N/2; ++i) {
        if (out_mag[i] > max_v) {
            max_v = out_mag[i];
            max_i = i;
        }
    }

    // verify the max bin matches k and energy concentration
    TEST_ASSERT_EQUAL_size_t(k, max_i);
    float total_energy = 0.0f;
    float energy_at_k = out_mag[k] * out_mag[k];
    for (size_t i = 0; i < N/2; ++i) {
        total_energy += out_mag[i] * out_mag[i];
    }
    TEST_ASSERT_TRUE((energy_at_k / total_energy) > 0.95f);
}

void test_fft_zero_input(void) {
    const size_t N = 256;
    float time_data[N] = {0};
    float out_mag[N/2];

    TEST_ASSERT_EQUAL_INT(0, fft_init(N));
    fft_compute(time_data, out_mag);
    for (size_t i = 0; i < N/2; ++i) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, out_mag[i]);
    }
}

void test_fft_impulse(void) {
    const size_t N = 256;
    float time_data[N] = {0};
    float out_mag[N/2];
    time_data[0] = 1.0f;

    TEST_ASSERT_EQUAL_INT(0, fft_init(N));
    fft_compute(time_data, out_mag);

    // impulse yields equal magnitude 1 in all bins
    for (size_t i = 0; i < N/2; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, out_mag[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fft_init_valid_size);
    RUN_TEST(test_fft_init_invalid_size);
    RUN_TEST(test_fft_sine_pure_bin5);
    RUN_TEST(test_fft_zero_input);
    RUN_TEST(test_fft_impulse);
    return UNITY_END();
}
