#include "vendor/unity/src/unity.h"
#include "../src/audio/audio_engine.h"
#include "vendor/unity/src/unity_internals.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FIXTURE_PATH "tests/fixtures/test_mono_44k.wav"

static WAV wav;

static void write_u16_le(FILE *f, uint16_t v) {
    unsigned char b[2];
    b[0] = (unsigned char)(v & 0xFFu);
    b[1] = (unsigned char)((v >> 8) & 0xFFu);
    fwrite(b, 1, sizeof(b), f);
}

static void write_u32_le(FILE *f, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xFFu);
    b[1] = (unsigned char)((v >> 8) & 0xFFu);
    b[2] = (unsigned char)((v >> 16) & 0xFFu);
    b[3] = (unsigned char)((v >> 24) & 0xFFu);
    fwrite(b, 1, sizeof(b), f);
}

static void write_wav_header(FILE *f,
                             uint16_t audio_format,
                             uint16_t channels,
                             uint32_t sample_rate,
                             uint16_t bits_per_sample,
                             uint32_t data_bytes) {
    uint16_t block_align = (uint16_t)(channels * (bits_per_sample / 8u));
    uint32_t byte_rate = sample_rate * (uint32_t)block_align;
    uint32_t riff_size = 4u + 8u + 16u + 8u + data_bytes;

    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, riff_size);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16u);
    write_u16_le(f, audio_format);
    write_u16_le(f, channels);
    write_u32_le(f, sample_rate);
    write_u32_le(f, byte_rate);
    write_u16_le(f, block_align);
    write_u16_le(f, bits_per_sample);

    fwrite("data", 1, 4, f);
    write_u32_le(f, data_bytes);
}

static int create_temp_wav_path(char *out_path, size_t out_path_len) {
    char tmp_template[] = "/tmp/bragi_wav_XXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd < 0) {
        return -1;
    }
    close(fd);

    strncpy(out_path, tmp_template, out_path_len - 1);
    out_path[out_path_len - 1] = '\0';
    return 0;
}

static int write_pcm24_wav(const char *path,
                           const int32_t *samples,
                           uint32_t frames,
                           uint16_t channels,
                           uint32_t sample_rate) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    uint32_t total_samples = frames * (uint32_t)channels;
    uint32_t data_bytes = total_samples * 3u;
    write_wav_header(f, 1u, channels, sample_rate, 24u, data_bytes);

    for (uint32_t i = 0; i < total_samples; ++i) {
        int32_t v = samples[i];
        unsigned char b[3];
        b[0] = (unsigned char)(v & 0xFF);
        b[1] = (unsigned char)((v >> 8) & 0xFF);
        b[2] = (unsigned char)((v >> 16) & 0xFF);
        fwrite(b, 1, sizeof(b), f);
    }

    fclose(f);
    return 0;
}

static int write_pcm32_wav(const char *path,
                           const int32_t *samples,
                           uint32_t frames,
                           uint16_t channels,
                           uint32_t sample_rate) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    uint32_t total_samples = frames * (uint32_t)channels;
    uint32_t data_bytes = total_samples * 4u;
    write_wav_header(f, 1u, channels, sample_rate, 32u, data_bytes);

    for (uint32_t i = 0; i < total_samples; ++i) {
        write_u32_le(f, (uint32_t)samples[i]);
    }

    fclose(f);
    return 0;
}

static int write_float32_wav(const char *path,
                             const float *samples,
                             uint32_t frames,
                             uint16_t channels,
                             uint32_t sample_rate) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    uint32_t total_samples = frames * (uint32_t)channels;
    uint32_t data_bytes = total_samples * 4u;
    write_wav_header(f, 3u, channels, sample_rate, 32u, data_bytes);

    for (uint32_t i = 0; i < total_samples; ++i) {
        uint32_t raw = 0;
        memcpy(&raw, &samples[i], sizeof(raw));
        write_u32_le(f, raw);
    }

    fclose(f);
    return 0;
}

void setUp(void) {
    memset(&wav, 0, sizeof(wav));
}

void tearDown(void) {
    if (wav.samples) wav_free(&wav);
}

void test_wav_load_success(void) {
    int err = wav_load(FIXTURE_PATH, &wav);
    TEST_ASSERT_EQUAL_INT(WAV_SUCCESS, err);
    TEST_ASSERT_EQUAL_UINT16(1, wav.audio_format);
    TEST_ASSERT_EQUAL_UINT16(1, wav.num_channels);
    TEST_ASSERT_EQUAL_UINT32(44100, wav.sample_rate);
    TEST_ASSERT_EQUAL_UINT16(16, wav.bits_per_sample);
    TEST_ASSERT_NOT_NULL(wav.samples);
    TEST_ASSERT_TRUE(wav.num_samples > 0);
}

void test_wav_load_missing_file(void) {
    int err = wav_load("does_not_exist.wav", &wav);
    TEST_ASSERT_EQUAL_INT(WAV_ERROR_OPEN_FAILED, err);
}

void test_wav_load_rejects_null_args(void) {
    TEST_ASSERT_EQUAL_INT(WAV_ERROR_INVALID_ARG, wav_load(NULL, &wav));
    TEST_ASSERT_EQUAL_INT(WAV_ERROR_INVALID_ARG, wav_load(FIXTURE_PATH, NULL));
}

void test_wav_free_handles_null(void) {
    wav_free(NULL);
    TEST_ASSERT_TRUE(1);
}

void test_wav_load_24bit_pcm(void) {
    char path[64];
    int32_t src[] = { -8388608, -4194304, 0, 4194304, 8388607 };

    TEST_ASSERT_EQUAL_INT(0, create_temp_wav_path(path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(0, write_pcm24_wav(path, src, 5, 1, 48000));

    int err = wav_load(path, &wav);
    unlink(path);

    TEST_ASSERT_EQUAL_INT(WAV_SUCCESS, err);
    TEST_ASSERT_EQUAL_UINT16(1u, wav.audio_format);
    TEST_ASSERT_EQUAL_UINT16(1u, wav.num_channels);
    TEST_ASSERT_EQUAL_UINT16(24u, wav.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(5u, wav.num_samples);

    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, wav.samples[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.5f, wav.samples[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, wav.samples[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, wav.samples[3]);
    TEST_ASSERT_FLOAT_WITHIN(2e-6f, 1.0f, wav.samples[4]);
}

void test_wav_load_32bit_pcm(void) {
    char path[64];
    int32_t src[] = { INT32_MIN, -(INT32_MAX / 2), 0, (INT32_MAX / 2), INT32_MAX };

    TEST_ASSERT_EQUAL_INT(0, create_temp_wav_path(path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(0, write_pcm32_wav(path, src, 5, 1, 48000));

    int err = wav_load(path, &wav);
    unlink(path);

    TEST_ASSERT_EQUAL_INT(WAV_SUCCESS, err);
    TEST_ASSERT_EQUAL_UINT16(1u, wav.audio_format);
    TEST_ASSERT_EQUAL_UINT16(1u, wav.num_channels);
    TEST_ASSERT_EQUAL_UINT16(32u, wav.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(5u, wav.num_samples);

    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, wav.samples[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.5f, wav.samples[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, wav.samples[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, wav.samples[3]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, wav.samples[4]);
}

void test_wav_load_32bit_float(void) {
    char path[64];
    float src[] = { -1.0f, -0.25f, 0.0f, 0.25f, 0.9f };

    TEST_ASSERT_EQUAL_INT(0, create_temp_wav_path(path, sizeof(path)));
    TEST_ASSERT_EQUAL_INT(0, write_float32_wav(path, src, 5, 1, 48000));

    int err = wav_load(path, &wav);
    unlink(path);

    TEST_ASSERT_EQUAL_INT(WAV_SUCCESS, err);
    TEST_ASSERT_EQUAL_UINT16(3u, wav.audio_format);
    TEST_ASSERT_EQUAL_UINT16(1u, wav.num_channels);
    TEST_ASSERT_EQUAL_UINT16(32u, wav.bits_per_sample);
    TEST_ASSERT_EQUAL_UINT32(5u, wav.num_samples);

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_FLOAT_WITHIN(1e-7f, src[i], wav.samples[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_load_success);
    RUN_TEST(test_wav_load_missing_file);
    RUN_TEST(test_wav_load_rejects_null_args);
    RUN_TEST(test_wav_free_handles_null);
    RUN_TEST(test_wav_load_24bit_pcm);
    RUN_TEST(test_wav_load_32bit_pcm);
    RUN_TEST(test_wav_load_32bit_float);
    return UNITY_END();
}
