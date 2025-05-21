// src/visualization/vis_utils.h

#ifndef VIS_UTILS_H
#define VIS_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert raw FFT magnitudes to normalized 0…1 dBFS values.
 *
 * @param raw_mag   Input array of length count: √(Re²+Im²) per bin.
 * @param out_db    Output array (length count) to hold normalized values.
 * @param count     Number of bins.
 * @param min_db    Lower clamp in dB (e.g. -80.0f).
 * @param max_db    Upper clamp in dB (e.g. 0.0f).
 */
void vis_convert_to_dbfs(const float *raw_mag,
                         float *out_db,
                         size_t count,
                         float min_db,
                         float max_db);

/**
 * Apply per-bin temporal smoothing (envelope follower) in place.
 *
 * @param data           Input/output array (length count).
 * @param prev_smooth    State array (length count) holding last frame’s values.
 * @param count          Number of bins.
 * @param alpha_attack   Attack smoothing factor in (0…1). Lower → faster rise.
 * @param alpha_decay    Decay smoothing factor in (0…1). Lower → faster fall.
 */
void vis_apply_temporal_smoothing(float *data,
                                  float *prev_smooth,
                                  size_t count,
                                  float alpha_attack,
                                  float alpha_decay);

/**
 * Apply simple spatial smoothing (moving average) in place.
 *
 * @param data        Input/output array (length count).
 * @param count       Number of bins.
 * @param window      Number of neighbors on each side (1 → 3-point average).
 */
void vis_apply_spatial_smoothing(float *data,
                                 size_t count,
                                 unsigned int window);

/**
 * Ease-out-cubic mapping: t↦1−(1−t)³, for a more “organic” falloff.
 *
 * @param t   Input in [0,1].
 * @return    Eased output in [0,1].
 */
float vis_ease_out_cubic(float t);

#ifdef __cplusplus
}
#endif

#endif /* VIS_UTILS_H */

