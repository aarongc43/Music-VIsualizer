#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <stdint.h>
#include <stddef.h>

/*hold PCM data and metatdata*/
typedef struct {
    uint16_t audio_format;      // PCM = 1
    uint16_t num_channels;      // 1=mono 2=stereo
    uint32_t sample_rate;       // e.g. 44100
    uint16_t bits_per_sample;   // total sample frames
    uint32_t num_samples;        // total number of sample frames
    float *samples;           // interleaved PCM samples
} WAV;

/**
 * load a 16-bit PCM WAV file
 * @param filepath path to .wav
 * @param out_wav pointer ot WAV struct to fill
 * @return 0 on success, non-zero on error
 */
int wav_load(const char *filepath, WAV *out_wav);

/** free the sample buffer inside a WAV */
void wav_free(WAV *wav);

/*
 * @brief Downmixes 'in_frames x channels' float samples into mono
 * in: [frame0_ch0, frame0_ch1, ..., frame1_ch0, ...]
 * out: [mono_frame0, mono_frame1, ...]
*/
void downmix_to_mono(const float *in, float *out, size_t frames, int channels);

#endif
