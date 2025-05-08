// src/audio/fft.h 

#ifndef FFT_H
#define FFT_H

#define FFT_SUCCESS                0
#define FFT_ERROR_BAD_SIZE        -1
#define FFT_ERROR_OOM             -2
#define FFT_ERROR_ALREADY_INIT    -3

#include <stddef.h>

/* initialize engine for size N. call only once */
int fft_init(size_t N);

/* compute magnitudes of real input array */
void fft_compute(const float *time_data, float *out_mag);

/* tear down, free buffers */
void fft_shutdown(void);

/* query initialized size; returns 0 if not initialized */
size_t fft_get_size(void);

#endif
