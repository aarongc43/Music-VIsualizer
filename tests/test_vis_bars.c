#include "vendor/unity/src/unity.h"
#include "../src/visualization/vis_bars.h"
#include <math.h>
#include <string.h>

#ifndef BAR_COUNT
#define BAR_COUNT 64
#endif
#ifndef BAR_MIN_H
#define BAR_MIN_H 2.0f
#endif

#define BIN_COUNT 2048
#define SCREEN_W 800
#define SCREEN_H 600
#define PEAK_VAL 1.0f
#define EPS 1e-4f

// Compute function prototype (already declared in vis_bars.h)
void vis_bars_compute(const float *mag, float *heights);

// Count how many bars exceed the minimum height (i.e. detected a peak)
static int peak_count(const float *h) {
    int c = 0;
    for (int i = 0; i < BAR_COUNT; ++i) {
        if (h[i] > BAR_MIN_H + EPS) ++c;
    }
    return c;
}

void setUp(void) {
    bars_shutdown();
}

void tearDown(void) {
    bars_shutdown();
}

// Verify initialization succeeds with a realistic bin count
void test_init_valid(void) {
    TEST_ASSERT_TRUE(bars_init(BIN_COUNT, SCREEN_W, SCREEN_H));
}

// A spike in a very low bin should light up the second bar (index 1)
void test_low_bin_maps_to_first_bar(void) {
    float mag[BIN_COUNT];
    float heights[BAR_COUNT];
    memset(mag, 0, sizeof(mag));
    memset(heights, 0, sizeof(heights));
    mag[5] = PEAK_VAL;

    bars_init(BIN_COUNT, SCREEN_W, SCREEN_H);
    vis_bars_compute(mag, heights);

    TEST_ASSERT_EQUAL_INT(1, peak_count(heights));
    int idx = -1;
    for (int i = 0; i < BAR_COUNT; ++i) {
        if (heights[i] > BAR_MIN_H + EPS) {
            idx = i;
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, idx);
}

// A spike in a mid-range bin is below the noise threshold → suppressed
void test_mid_bin_suppressed_by_threshold(void) {
    float mag[BIN_COUNT];
    float heights[BAR_COUNT];
    memset(mag, 0, sizeof(mag));
    memset(heights, 0, sizeof(heights));
    mag[BIN_COUNT / 4] = PEAK_VAL;

    bars_init(BIN_COUNT, SCREEN_W, SCREEN_H);
    vis_bars_compute(mag, heights);

    TEST_ASSERT_EQUAL_INT(0, peak_count(heights));
}

// A spike in a high-frequency bin is also suppressed
void test_high_bin_suppressed_by_threshold(void) {
    float mag[BIN_COUNT];
    float heights[BAR_COUNT];
    memset(mag, 0, sizeof(mag));
    memset(heights, 0, sizeof(heights));
    mag[BIN_COUNT - 10] = PEAK_VAL;

    bars_init(BIN_COUNT, SCREEN_W, SCREEN_H);
    vis_bars_compute(mag, heights);

    TEST_ASSERT_EQUAL_INT(0, peak_count(heights));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_valid);
    RUN_TEST(test_low_bin_maps_to_first_bar);
    RUN_TEST(test_mid_bin_suppressed_by_threshold);
    RUN_TEST(test_high_bin_suppressed_by_threshold);
    return UNITY_END();
}

