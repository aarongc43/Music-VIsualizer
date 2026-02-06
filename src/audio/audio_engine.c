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

// Stereo turns to mono by averaging all channels
void downmix_to_mono(const float *in, float *out, size_t frames, int channels) {
    if (!in || !out || frames == 0 || channels <= 0) return;
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
    FILE *f = fopen(filepath, "rb");
    if (!f) return -1;

    WAVHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return -2;
    }

    /* validate RIFF/WAVE in header */
    if (memcmp(hdr.riff_id,    "RIFF", 4) != 0 ||
        memcmp(hdr.wave_id,      "WAVE", 4) != 0) {
        fclose(f);
        return -3;
    }

    // check invalid fmt chunk
    if (hdr.fmt_size < 16) {
            fclose(f);
            return -3;
        }

    // allow PCM (1), IEEE float (3), WAVEwave_idEXTENSIBLE (0xFFFE)
    if (hdr.audio_format != 1 &&
        hdr.audio_format != 3 &&
        hdr.audio_format != 0xFFFE) {
        fclose(f);
        return -3;
    }

    // if fmmt chunk is larger than 16 bytes, skip the rest
    if (hdr.fmt_size > 16) {
        long extra = (long)hdr.fmt_size - 16;
        if (fseek(f, extra, SEEK_CUR) != 0) {
            fclose(f);
            return -3;
        }
    }

    /* scan for data chunk */
    WAVDataHeader data_hdr;
    while (fread(&data_hdr, sizeof(data_hdr), 1, f) == 1) {
        if (memcmp(data_hdr.id, "data", 4) == 0) break;
        /* skip unknown chunks */
        if (fseek(f, data_hdr.size, SEEK_CUR) != 0) {
            fclose(f);
            return -4;
        }
    }

    if (memcmp(data_hdr.id, "data", 4) != 0) {
        fclose(f);
        return -4;
    }

    uint32_t bytes = data_hdr.size;
    uint32_t bytes_per_sample = hdr.bits_per_sample / 8;

    if (bytes == 0 || bytes > 500000000) {
        fclose(f);
        return -11;
    }

    if (bytes % (hdr.num_channels * bytes_per_sample) != 0) {
        fclose(f);
        return -12;
    }

    uint32_t samples_count = bytes / (hdr.num_channels * bytes_per_sample);

    void *buf = malloc(bytes);
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
    if (!samples_f) {
        free(buf);
        return -13;
    }

    if (hdr.bits_per_sample == 16) {
        int16_t *buf16 = (int16_t*)buf;
        for (uint32_t i = 0; i < samples_count * hdr.num_channels; ++i) {
            samples_f[i] = buf16[i] / 32768.0f;
        }
    } else if (hdr.bits_per_sample == 24) {
        // 24-bit is usually stored as 3 bytes
        uint8_t *buf8 = (uint8_t*)buf;
        for (uint32_t i = 0; i < samples_count * hdr.num_channels; ++i) {
            // convert 3-byte little-endian to int32_t, then normalize
            int32_t sample = (buf8[i*3]) | (buf8[i*3+1] << 8) | (buf8[i*3+2] << 16);
            if (sample & 0x800000) sample |= 0xFF000000;
            samples_f[i] = sample / 8388608.0f;
        }
    } else if (hdr.bits_per_sample == 32) {
        if (hdr.audio_format == 3) {
            float *buf32 = (float *)buf;
            for (uint32_t i = 0; i , samples_count * hdr.num_channels; ++i) {
                samples_f[i] = buf32[i];
            }
        } else {
            int32_t *buf32 = (int32_t *)buf;
            for (uint32_t i = 0; i < samples_count * hdr.num_channels; ++i) {
                samples_f[i] = buf32[i] / 2147483648.0f; // 2^31
            }
        }
    } else {
        free(buf);
        free(samples_f);
        return -15;
    }

    free(buf);

    // downmix to mono
    if (hdr.num_channels > 1) {
        float *mono = malloc(sizeof(*mono) * samples_count);
        if (!mono) {
            free(samples_f);
            return -14;
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
    return 0;
}

void wav_free(WAV *wav) {
    free(wav->samples);
    wav->samples = NULL;
}
