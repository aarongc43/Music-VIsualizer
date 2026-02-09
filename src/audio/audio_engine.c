#include "audio_engine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://www.mmsp.ece.mcgill.ca/Documents/Audiowave_ids/WAVE/WAVE.html

#pragma pack(push,1)

// WAV header file 44 bytes
typedef struct {
    char        riff_id[4];         // "RIFF"
    uint32_t    riff_size;          // size of entire file
    char        wave_id[4];         // "WAVE"
    char        fmt_id[4];          // "fmt"
    uint32_t    fmt_size;           // 16 for PCM
    uint16_t    audio_format;       // PCM = 1
    uint16_t    num_channels;       // mono or stereo
    uint32_t    sample_rate;        // samples per second
    uint32_t    byte_rate;          // bytes per second
    uint16_t    block_align;        // bytes per sample frame
    uint16_t    bits_per_sample;    // bits per samples
} WAVHeader;

typedef struct {
    char        id[4];    // "data"
    uint32_t    size;     // data bytes
} WAVDataHeader;
#pragma pack(pop)

#define WAV_MAX_DATA_BYTES 500000000u

// Stereo turns to mono by averaging all channels
void downmix_to_mono(const float *in, float *out, size_t frames, int channels) {
    if (!in || !out || frames == 0 || channels <= 0) {
        return;
    }

    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        const float *frame_ptr = in + i * channels;
        for (int ch = 0; ch < channels; ++ch) {
            sum += frame_ptr[ch];
        }
        out[i] = sum / (float)channels;
    }
}

int wav_load(const char *filepath, WAV *out_wav) {
    if (!filepath || !out_wav) {
        return WAV_ERROR_INVALID_ARG;
    }

    memset(out_wav, 0, sizeof(*out_wav));

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return WAV_ERROR_OPEN_FAILED;
    }

    WAVHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return WAV_ERROR_READ_HEADER;
    }

    /* validate RIFF/WAVE in header */
    if (memcmp(hdr.riff_id, "RIFF", 4) != 0 ||
        memcmp(hdr.wave_id, "WAVE", 4) != 0 ||
        memcmp(hdr.fmt_id, "fmt ", 4) != 0) {
        fclose(f);
        return WAV_ERROR_BAD_HEADER;
    }

    // check invalid fmt chunk
    if (hdr.fmt_size < 16) {
        fclose(f);
        return WAV_ERROR_BAD_HEADER;
    }

    if (hdr.num_channels == 0 || hdr.sample_rate == 0) {
        fclose(f);
        return WAV_ERROR_BAD_HEADER;
    }

    // allow PCM (1) or IEEE float (3)
    if (hdr.audio_format != 1 &&
        hdr.audio_format != 3) {
        fclose(f);
        return WAV_ERROR_UNSUPPORTED_FORMAT;
    }

    if (hdr.bits_per_sample != 16 &&
        hdr.bits_per_sample != 24 &&
        hdr.bits_per_sample != 32) {
        fclose(f);
        return WAV_ERROR_UNSUPPORTED_FORMAT;
    }

    if (hdr.audio_format == 3 && hdr.bits_per_sample != 32) {
        fclose(f);
        return WAV_ERROR_UNSUPPORTED_FORMAT;
    }

    // if fmt chunk is larger than 16 bytes, skip the rest
    if (hdr.fmt_size > 16) {
        long extra = (long)hdr.fmt_size - 16;
        if (fseek(f, extra, SEEK_CUR) != 0) {
            fclose(f);
            return WAV_ERROR_SEEK_FAILED;
        }
    }

    /* scan for data chunk */
    WAVDataHeader data_hdr;
    int data_found = 0;
    while (fread(&data_hdr, sizeof(data_hdr), 1, f) == 1) {
        if (memcmp(data_hdr.id, "data", 4) == 0) {
            data_found = 1;
            break;
        }

        /* skip unknown chunks */
        long skip = (long)data_hdr.size + (long)(data_hdr.size & 1u);
        if (fseek(f, skip, SEEK_CUR) != 0) {
            fclose(f);
            return WAV_ERROR_SEEK_FAILED;
        }
    }

    if (!data_found) {
        fclose(f);
        return WAV_ERROR_DATA_CHUNK_NOT_FOUND;
    }

    uint32_t bytes = data_hdr.size;
    uint32_t bytes_per_sample = hdr.bits_per_sample / 8;
    uint64_t frame_bytes = (uint64_t)hdr.num_channels * (uint64_t)bytes_per_sample;

    if (bytes == 0 || bytes > WAV_MAX_DATA_BYTES) {
        fclose(f);
        return WAV_ERROR_DATA_SIZE;
    }

    if (bytes_per_sample == 0 || frame_bytes == 0 || (bytes % frame_bytes) != 0) {
        fclose(f);
        return WAV_ERROR_DATA_SIZE;
    }

    uint32_t samples_count = (uint32_t)(bytes / frame_bytes);

    size_t total_samples = (size_t)samples_count * (size_t)hdr.num_channels;
    if ((uint64_t)samples_count * (uint64_t)hdr.num_channels > SIZE_MAX / sizeof(float)) {
        fclose(f);
        return WAV_ERROR_ALLOC;
    }

    void *buf = malloc((size_t)bytes);
    if (!buf) {
        fclose(f);
        return WAV_ERROR_ALLOC;
    }

    if (fread(buf, 1, (size_t)bytes, f) != (size_t)bytes) {
        free(buf);
        fclose(f);
        return WAV_ERROR_READ_DATA;
    }
    fclose(f);

    float *samples_f = malloc(sizeof(*samples_f) * total_samples);
    if (!samples_f) {
        free(buf);
        return WAV_ERROR_ALLOC;
    }

    if (hdr.bits_per_sample == 16) {
        int16_t *buf16 = (int16_t*)buf;
        for (size_t i = 0; i < total_samples; ++i) {
            samples_f[i] = buf16[i] / 32768.0f;
        }
    } else if (hdr.bits_per_sample == 24) {
        // 24-bit is usually stored as 3 bytes
        uint8_t *buf8 = (uint8_t*)buf;
        for (size_t i = 0; i < total_samples; ++i) {
            // convert 3-byte little-endian to int32_t, then normalize
            int32_t sample = (buf8[i*3]) | (buf8[i*3+1] << 8) | (buf8[i*3+2] << 16);
            if (sample & 0x800000) {
                sample |= 0xFF000000;
            }
            samples_f[i] = sample / 8388608.0f;
        }
    } else if (hdr.bits_per_sample == 32) {
        if (hdr.audio_format == 3) {
            float *buf32 = (float *)buf;
            for (size_t i = 0; i < total_samples; ++i) {
                samples_f[i] = buf32[i];
            }
        } else {
            int32_t *buf32 = (int32_t *)buf;
            for (size_t i = 0; i < total_samples; ++i) {
                samples_f[i] = buf32[i] / 2147483648.0f; // 2^31
            }
        }
    } else {
        free(buf);
        free(samples_f);
        return WAV_ERROR_UNSUPPORTED_FORMAT;
    }

    free(buf);

    // downmix to mono
    if (hdr.num_channels > 1) {
        float *mono = malloc(sizeof(*mono) * samples_count);
        if (!mono) {
            free(samples_f);
            return WAV_ERROR_ALLOC;
        }

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
    return WAV_SUCCESS;
}

void wav_free(WAV *wav) {
    if (!wav) {
        return;
    }

    free(wav->samples);
    wav->samples = NULL;
}
