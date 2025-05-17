// src/visualization/vis_circles.h 

#ifndef VIS_CIRCLES_H
#define VIS_CIRCLES_H

#include <stddef.h>
#include <stdbool.h>
#include <raylib.h>

/**
    * @brief Init circles renderer
    *
    * @param bin_count Number of FFT bins (fft/2)
    * @param screen_w Window width in pixels
    * @param screen_h Window height in pixels
    * @return true on success, false on allocation failure
*/

bool cirlces_init(size_t bin_count, int screen_w, int screen_h);

/**
    * @brief Draw pulsing concentric circles using a subset of bins
    *
    * Should be called between BeginDrawing() and EndDrawing()
    *
    * @param magnitudes Array of length bin_count with non-negative floats
*/
void circles_render(const float *magnitudes);

/**
    * @brief Release all resources held by circles renderer
*/
void circles_shutdown(void);

#endif
