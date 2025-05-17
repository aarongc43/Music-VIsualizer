// src/visualization/v_engine.h 

#ifndef VISUALIZATION_ENGINE_H
#define VISUALIZATION_ENGINE_H

#include <stddef.h>
#include <stdbool.h>

/**
    * @brief initialize visualization engine
    * 
    * @param screen_width   width of rendering window
    * @param screen_height  height of rendering window
    * @param fft_size       FFT size (must match fft_init's N); vis_bins = fft_size/2
    * @return true on success, false on any allocation or init failure
*/

bool vis_init(int screen_width, int screen_height, size_t fft_size);

/**
    * @brief  provide a fresh buffer of FFT magnitudes
    *
    * called by event handler whenever new FFT data is ready.
    * just a memcpy, no memory allocation
    *
    * @param magnitudes Array of length bin_count (== fft_size/2)
    * @param bin_count  Number of bins in the array
*/
void vis_update(const float *magnitudes, size_t bin_count);

/**
    * @brief Render the current frame's visualization
    *
    * Must be called between BeginDrawing() and EndDrawing()
*/
void vis_render(void);

/**
    * @brief Tear down all visualization resources
    *
    * Free buffers, unregister any hooks. Called at shutdown
*/
void vis_shutdown();

#endif
