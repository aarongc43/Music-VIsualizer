#include "fft.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

static size_t g_fft_size = 0;
static uint32_t *g_bitrev_table = NULL;     // length of N 
static float *g_twiddle_table = NULL;       // length of N/2 * 2
static float *g_data;                       // length = 2 * g_fft_size

int fft_init(size_t N) {
    // validate N: must be power of two between 128 and 8192
    if (N < 128 || N > 8192) {
        return -1;
    }

    // power-of-two check
    if ((N &(N - 1)) != 0) {
        return -1;
    }

    // allocate bit-rev table (N entries)
    size_t bytes_rev = N *sizeof(uint32_t);
    if (posix_memalign((void**)&g_bitrev_table, 64, bytes_rev) != 0) {
        return -2;
    }

    size_t half = N >> 1;
    size_t bytes_tw = half * 2 * sizeof(float);
    if (posix_memalign((void**)&g_twiddle_table, 64, bytes_tw) != 0) {
        free(g_bitrev_table);
        g_bitrev_table = NULL;
        return -2;
    }

    int levels = __builtin_ctz(N);      // count trailing zeros == log2(N)
    for (size_t i = 0; i < N; ++i) {
        // reverse the low 'levels' bits of i 
        uint32_t rev = __builtin_bitreverse32(i) >> (32 - levels);
        g_bitrev_table[i] = rev;
    }

    // portable c 
    /*
    for (size_t i = 0; i < N; ++i) {
        uint32_t rev = 0, x = i;
        for (int b = 0; b < levels; ++b) {
            rev = (rev << 1) | (x & 1);
            x  >>= 1;
        }
        g_bitrev_table[i] = rev;
    }
    */

    // compute twiddle factors
    const float angle_step = -2.0f * M_PI / (float)N;
    for (size_t k = 0; k < half; ++k) {
        float ang = k * angle_step;
        // twiddle[2*k] = cos(ang)
        // twiddle[2*k + 1] = sinf(ang)
        g_twiddle_table[2*k] = cosf(ang);
        g_twiddle_table[2*k + 1] = sinf(ang);
    }

    posix_memalign((void**)&g_data, 64, 2 * N * sizeof(float));

    g_fft_size = N;
    return 0;

}

void fft_compute(const float *time_data, float *out_mag) {
    for (size_t i = 0; i < N; ++i) {
        uint32_t j = g_bitrev_table[i];
        // real part
        g_data[2*j] = time_data[i];
        // imaginary part = 0
        g_data[2*j + 1] = 0.0f;
    }
}

void fft_shutdown(void) {
    free(g_bitrev_table);
    free(g_twiddle_table);
    free(g_data);
    g_bitrev_table = NULL;
    g_twiddle_table = NULL;
    g_data = NULL;
    g_fft_size = 0;
}

size_t fft_get_size(void) {
    return g_fft_size;
}
