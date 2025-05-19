#include "vendor/unity/src/unity.h"
#include "../src/visualization/visualization_engine.h"
#include "vendor/unity/src/unity_internals.h"

void setUp(void)   { /* nothing */ }
void tearDown(void){ /* nothing */ }

void test_vis_init_invalid_size(void) {
    // fft_size = 0 → bin_count = 0 → should fail
    TEST_ASSERT_FALSE( vis_init(640, 480, 0) );
}

void test_vis_init_valid(void) {
    // fft_size = 1024 → bin_count = 512
    TEST_ASSERT_TRUE( vis_init(800, 600, 1024) );
    vis_shutdown();
}

void test_vis_update_mismatched_bins(void) {
    TEST_ASSERT_TRUE( vis_init(800, 600, 1024) );
    float data[511] = {0};
    // Should do nothing (and not crash)
    vis_update(data, 511);
    vis_shutdown();
}

void test_vis_update_and_swap(void) {
    TEST_ASSERT_TRUE( vis_init(320, 240, 16) ); // bin_count = 8
    float src[8];
    for (int i = 0; i < 8; i++) src[i] = (float)(i+1);
    // Copy into the back buffer
    vis_update(src, 8);
    // We need a way to peek at the front buffer to assert it matches.
    // (In real tests we might expose a test‐only accessor or rely on no-crash.)
    vis_shutdown();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_vis_init_invalid_size);
    RUN_TEST(test_vis_init_valid);
    RUN_TEST(test_vis_update_mismatched_bins);
    // RUN_TEST(test_vis_update_and_swap); // if you add an accessor
    return UNITY_END();
}
