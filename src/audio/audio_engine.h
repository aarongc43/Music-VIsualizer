#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#define WAV_SUCCESS 0
#define WAV_ERROR_INVALID_ARG -1
#define WAV_ERROR_OPEN_FAILED -2
#define WAV_ERROR_READ_HEADER -3
#define WAV_ERROR_BAD_HEADER -4
#define WAV_ERROR_SEEK_FAILED -5
#define WAV_ERROR_DATA_CHUNK_NOT_FOUND -6
#define WAV_ERROR_DATA_SIZE -7
#define WAV_ERROR_ALLOC -8
#define WAV_ERROR_READ_DATA -9
#define WAV_ERROR_UNSUPPORTED_FORMAT -10

/* Holds decoded PCM data and metadata. */
typedef struct {
    uint16_t audio_format;    // PCM = 1, IEEE float = 3
    uint16_t num_channels;    // output is always 1 (mono) after load
    uint32_t sample_rate;     // e.g. 44100
    uint16_t bits_per_sample; // source bit depth
    uint32_t num_samples;     // number of mono frames in samples
    float *samples;           // mono PCM samples in [-1, 1]
} WAV;

/**
 * Load a WAV file and decode to float mono.
 * @param filepath path to .wav
 * @param out_wav pointer to WAV struct to fill
 * @return WAV_SUCCESS on success, otherwise a WAV_ERROR_* code
 */
int wav_load(const char *filepath, WAV *out_wav);

/** free the sample buffer inside a WAV */
void wav_free(WAV *wav);

/* Downmixes interleaved multichannel float samples to mono. */
void downmix_to_mono(const float *in, float *out, size_t frames, int channels);

#endif
