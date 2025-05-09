 // tools/fft_bench.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../audio/fft/fft.h"

static double timespec_diff_us(const struct timespec *a, const struct timespec *b) {
     return (b->tv_sec - a->tv_sec) * 1e6 +
            (b->tv_nsec - a->tv_nsec) / 1e3;
 }

 void run_benchmark(size_t N) {
    const int runs = 1000;
    float *in      = malloc(sizeof(float) * N);
    float *out_mag = malloc(sizeof(float) * (N/2));
    if (!in || !out_mag) {
        fprintf(stderr, "OOM allocating buffers\\n");
        exit(1);
    }

    // init and prefill with dummy data (e.g. zeros)
    if (fft_init(N) != FFT_SUCCESS) {
        fprintf(stderr, "fft_init(%zu) failed\\n", N);
        exit(1);
    }
    for (size_t i = 0; i < N; ++i) in[i] = 0.0f;

    struct timespec t0, t1;
    double total_us = 0, max_us = 0;
    for (int i = 0; i < runs; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        fft_compute(in, out_mag);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double dt = timespec_diff_us(&t0, &t1);
        total_us += dt;
        if (dt > max_us) max_us = dt;
    }

    printf("FFT %4zu: mean = %8.2f µs, max = %8.2f µs over %d runs\\n",
           N, total_us / runs, max_us, runs);

    fft_shutdown();
    free(in);
    free(out_mag);
}

int main(void) {
    run_benchmark(1024);
    run_benchmark(2048);
    return 0;
}
