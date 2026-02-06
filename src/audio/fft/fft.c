#include "fft.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

static size_t    g_fft_size         = 0;
static uint32_t *g_bitrev_table     = NULL;     // length of N 
static float    *g_twiddle_table    = NULL;     // length of N/2 * 2
static float    *g_data;                        // length = 2 * g_fft_size
static float    *g_window_buf       = NULL;

int fft_init(size_t N) {
    if (g_fft_size != 0) {
        // Already initialized, caller must call fft_shutdown() first
        g_fft_size = N;
    }

    // validate N: must be power of two between 128 and 8192
    if (N < 128 || N > 8192) {
        return FFT_ERROR_BAD_SIZE;
    }

    // power-of-two check
    if ((N &(N - 1)) != 0) {
        return FFT_ERROR_BAD_SIZE;
    }

    // allocate bit-rev table (N entries)
    size_t bytes_rev = N *sizeof(uint32_t);
    uint32_t *tmp_rev = NULL;
    if (posix_memalign((void**)&tmp_rev, 64, bytes_rev) != 0) {
        return FFT_ERROR_OOM;
    }
    g_bitrev_table = tmp_rev;

    size_t half = N >> 1;
    size_t bytes_tw = half * 2 * sizeof(float);
    float *tmp_tw = NULL;
    if (posix_memalign((void**)&tmp_tw, 64, bytes_tw) != 0) {
        free(g_bitrev_table);
        g_bitrev_table = NULL;
        return FFT_ERROR_OOM;
    }
    g_twiddle_table = tmp_tw;

    int levels = __builtin_ctz(N);      // count trailing zeros == log2(N)
    for (size_t i = 0; i < N; ++i) {
        // reverse the low 'levels' bits of i 
        uint32_t rev = __builtin_bitreverse32(i) >> (32 - levels);
        g_bitrev_table[i] = rev;
    }

    // compute twiddle factors
    const float angle_step = -2.0f * M_PI / (float)N;
    for (size_t k = 0; k < half; ++k) {
        float ang = k * angle_step;
        // twiddle[2*k] = cos(ang)
        // twiddle[2*k + 1] = sinf(ang)
        g_twiddle_table[2*k] = cosf(ang);
        g_twiddle_table[2*k + 1] = sinf(ang);
    }

    float *tmp_data = NULL;
    if (posix_memalign((void**)&tmp_data, 64, 2 * N * sizeof(float)) != 0) {
        free(g_bitrev_table);
        free(g_twiddle_table);
        g_bitrev_table = NULL;
        g_twiddle_table = NULL;
        return FFT_ERROR_OOM;
    }
    g_data = tmp_data;

    // allocate window buffer
    g_window_buf = malloc(N * sizeof(float));
    if (!g_window_buf) {
        free(g_bitrev_table);
        free(g_twiddle_table);
        free(g_data);
        g_bitrev_table = NULL;
        g_twiddle_table = NULL;
        g_data = NULL;
        return FFT_ERROR_OOM;
    }

    g_fft_size = N;
    return FFT_SUCCESS;
}

void fft_compute_raw(const float *time_data, float *out_mag) {
    assert(g_fft_size > 0   && "fft_compute() called before fft_init()");
    assert(time_data        && "fft_compute(): time_data is NULL");
    assert(out_mag          && "fft_compute(): out_mag is NULL");

    size_t N = g_fft_size;
    if (!N || !time_data || !out_mag) return;

    for (size_t i = 0; i < N; ++i) {
        // bit-reversed index
        uint32_t j = g_bitrev_table[i];

        // real part
        g_data[2*j] = time_data[i];
        // imaginary part = 0
        g_data[2*j + 1] = 0.0f;
    }

    int levels = __builtin_ctz(N);

    for (int s = 1; s <= levels; ++s) {
        size_t m    = 1u << s;
        size_t half = m >> 1;
        size_t step = N / m;

        for (size_t k = 0; k < N; k += m) {
            for (size_t j = 0; j < half; ++j) {
                size_t idx_even = k + j;
                size_t idx_odd = idx_even + half;

                // twiddle index = j * step
                float wr = g_twiddle_table[2 * (j * step)];
                float wi = g_twiddle_table[2 * (j * step) + 1];

                float ur = g_data[2 * idx_even];
                float ui = g_data[2 * idx_even + 1];
                float vr = g_data[2 * idx_odd];
                float vi = g_data[2 * idx_odd + 1];

                // t = W * v
                float tr = wr * vr - wi * vi;
                float ti = wr * vi + wi * vr;

                // u + t, u -t
                g_data[2 * idx_even] =      ur + tr;
                g_data[2 * idx_even + 1] =  ui + ti;
                g_data[2 * idx_odd] =       ur - tr;
                g_data[2 * idx_odd + 1] =   ui - ti;
            }
        }
    }

    // For real-input FFT, only bins 0…N/2−1 are unique
    size_t halfN = N / 2 + 1;
    for (size_t k = 0; k < halfN; ++k) {
        float re = g_data[2*k];
        float im = g_data[2*k + 1];
        out_mag[k] = sqrtf(re * re + im * im);
    }
}

void fft_compute(const float *time_data, float *out_mag) {
    size_t N = g_fft_size;

    // hann window
    for (size_t i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (N - 1)));
        g_window_buf[i] = time_data[i] * w;
    }

    fft_compute_raw(g_window_buf, out_mag);

    // Normalize magnitudes to approximate 0 dBFS for a full-scale sine wave
    // Single-sided spectrum scaling: 2/N, and compensate Hann coherent gain (~0.5)
    // => overall scale ≈ 4/N across bins
    const float scale = (N > 0) ? (4.0f / (float)N) : 0.0f;
    size_t halfN = N >> 1;
    for (size_t k = 0; k < halfN; ++k) {
        out_mag[k] *= scale;
    }
}

void fft_shutdown(void) {
    free(g_window_buf);
    g_window_buf = NULL;
    free(g_bitrev_table);
    free(g_twiddle_table);
    free(g_data);
    g_bitrev_table = NULL;
    g_twiddle_table = NULL;
    g_data = NULL;
}

size_t fft_get_size(void) {
    return g_fft_size;
}
