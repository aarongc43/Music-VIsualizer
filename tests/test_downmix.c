#include "vendor/unity/src/unity.h"
#include "../src/audio/audio_engine.h"

void setUp(void) {}
void tearDown(void) {}

void test_downmix_simple_stereo(void) {
    float in[]  = { 1, 0,  0, 1,  1, 1 }; // 3 frames, 2 channels
    float out[3];
    downmix_to_mono(in, out, 3, 2);
    TEST_ASSERT_FLOAT_WITHIN(1e-6, 0.5f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6, 0.5f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6, 1.0f, out[2]);
}

void test_downmix_three_channels(void) {
    float in[]  = { 1, 2, 3,  4, 5, 6 }; // 2 frames, 3 channels
    float out[2];
    downmix_to_mono(in, out, 2, 3);
    TEST_ASSERT_FLOAT_WITHIN(1e-6, 2.0f, out[0]); // (1+2+3)/3
    TEST_ASSERT_FLOAT_WITHIN(1e-6, 5.0f, out[1]); // (4+5+6)/3
}
