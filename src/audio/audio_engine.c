#include "audio_engine.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

#pragma pack(push,1)

// WAV header file 44 bytes
typedef struct {
    char        chunk_id[4];        // "RIFF"
    uint32_t    chunk_size;         // size of entire file
    char        format[4];          // "WAVE"
    //format subchunk - describes audio format
    char        subchunck1_id[4];   // "fmt"
    uint32_t    subchunk1_size;     // 16 for PCM
    uint16_t    audio_format;       // PCM = 1
    uint16_t    num_channels;       // mono or stereo
    uint32_t    sample_rate;        // samples per second
    uint32_t    byte_rate;          // bytes per second
    uint16_t    block_align;        // bytes per sample frame
    uint16_t    bits_per_sample;    // bits per samples
} WAVHeader;

typedef struct {
    char        subchunk2_id[4];    // "data"
    uint32_t    subchunk2_size;     // data bytes
} WAVDataHeader;
#pragma pack(pop)

// Stereo turns to mono by averaging all channels
void downmix_to_mono(const float *in, float *out, size_t frames, int channels) {
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        const float *frame_ptr = in + i * channels;

        // sum  all channels in the frame
        for (int ch = 0; ch < channels; ++ch)
            sum += frame_ptr[ch];
        // averate out channels to create the mono sample
        out[i] = sum / channels;
    }
}

int wav_load(const char *filepath, WAV *out_wav) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return -1;

    WAVHeader hdr;
    /*
     * size_t items_read = fread (
     * void *ptr,       // where to store bytes
     * size_t size,     // size of each item
     * size_t count,    // number of items
     * FILE *stream     // file handle
     * )
    */
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return -2;
    }

    /* validate RIFF/WAVE in header */
    if (memcmp(hdr.chunk_id, "RIFF", 4) != 0 ||
        memcmp(hdr.format, "WAVE", 4) != 0 ||
        hdr.audio_format != 1 /* PCM format */) {
        fclose(f);
        return -3;
    }

    /* skip to "data" subchunk */
    WAVDataHeader data_hdr;
    while (fread(&data_hdr, sizeof(data_hdr), 1, f) == 1) {
        if (memcmp(data_hdr.subchunk2_id, "data", 4) == 0) break;
        /* skip unknown chunks */
        fseek(f, data_hdr.subchunk2_size, SEEK_CUR);
    }

    if (memcmp(data_hdr.subchunk2_id, "data", 4) != 0) {
        fclose(f);
        return -4;
    }

    uint32_t bytes = data_hdr.subchunk2_size;
    uint32_t samples_count = bytes / (hdr.num_channels * (hdr.bits_per_sample/8));
    int16_t *buf = malloc(bytes);
    if (!buf) {
        fclose(f);
        return -5;
    }

    if (fread(buf, 1, bytes, f) != bytes) {
        free(buf);
        fclose(f);
        return -6;
    }
    fclose(f);

    float *samples_f = malloc(sizeof(*samples_f) * samples_count * hdr.num_channels);
    for (uint32_t i = 0; i < samples_count * hdr.num_channels; ++i) {
        samples_f[i] = buf[i] / 32768.0f;
    }
    free(buf);

    if (hdr.num_channels > 1) {
        float *mono = malloc(sizeof(*mono) * samples_count);
        downmix_to_mono(samples_f, mono, samples_count, hdr.num_channels);
        free(samples_f);
        samples_f = mono;
        hdr.num_channels = 1;
    }

    /* populate out_wave */
    out_wav->audio_format   = hdr.audio_format;
    out_wav->num_channels   = hdr.num_channels;
    out_wav->sample_rate    = hdr.sample_rate;
    out_wav->bits_per_sample= hdr.bits_per_sample;
    out_wav->num_samples    = samples_count;
    out_wav->samples        = samples_f;
    return 0;
}

void wav_free(WAV *wav) {
    free(wav->samples);
    wav->samples = NULL;
}
