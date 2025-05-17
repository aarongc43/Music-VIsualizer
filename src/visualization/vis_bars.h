// src/visualization/vis_bars.h 

#ifndef VIS_BARS_H
#define VIS_BARS_H

#include <stddef.h>
#include <stdbool.h>
#include <raylib.h>

/**
    * @brief Initialize the bars renderer
    *
    * @param bin_count  Number of FFT bins (fft_size/2)
    * @param screen_w   Window width in pixels
    * @param screen_h   Window height in pixels
    * @return true on success, flase on allocation failure
*/
bool bars_init(size_t bin_count, int screen_w, int screen_h);

/**
    * @brief Draw vertical bars for each FFT bin
    *
    * should be called between BeginDrawing() and EndDrawing()
    *
    * @param magnitudes Array of length bin_count with non-negative floats
*/
void bars_render(const float *magnitudes);

/**
    * @brief Release all resources held by the bars renderer
*/
void bars_shutdown(void); 

#endif
