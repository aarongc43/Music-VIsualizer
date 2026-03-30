// src/visualization/vis_utils.c

#include "vis_utils.h"
#include <math.h>
#include <string.h>

void vis_convert_to_dbfs(const float *raw_mag,
                         float *out_db,
                         size_t count,
                         float min_db,
                         float max_db)
{
    const float eps = 1e-12f;
    const float inv_range = 1.0f / (max_db - min_db);

    for (size_t k = 0; k < count; ++k) {
        // 20·log10(mag) → dBFS
        float db = 20.0f * log10f(raw_mag[k] + eps);
        // clamp
        if (db < min_db) db = min_db;
        else if (db > max_db) db = max_db;
        // normalize to [0,1]
        out_db[k] = (db - min_db) * inv_range;
    }
}

void vis_apply_temporal_smoothing(float *data,
                                  float *prev_smooth,
                                  size_t count,
                                  float alpha_attack,
                                  float alpha_decay)
{
    for (size_t k = 0; k < count; ++k) {
        float x = data[k];
        float y = prev_smooth[k];
        if (x > y) {
            // rising edge: use faster attack (lower α)
            y = alpha_attack * y + (1.0f - alpha_attack) * x;
        } else {
            // falling edge: slower decay (higher α)
            y = alpha_decay * y + (1.0f - alpha_decay) * x;
        }
        prev_smooth[k] = y;
        data[k] = y;
    }
}

void vis_apply_spatial_smoothing(float *data,
                                 size_t count,
                                 unsigned int window)
{
    if (window == 0 || count < 2) return;
    float temp[count];
    memcpy(temp, data, count * sizeof(float));

    // Process every bin — clamp neighbor indices to the array bounds at the edges
    // so the first and last bins are smoothed rather than left untouched.
    for (size_t k = 0; k < count; ++k) {
        float sum = 0.0f;
        for (int j = -(int)window; j <= (int)window; ++j) {
            int idx = (int)k + j;
            if (idx < 0)             idx = 0;
            else if (idx >= (int)count) idx = (int)count - 1;
            sum += data[idx];
        }
        temp[k] = sum / (2*window + 1);
    }
    memcpy(data, temp, count * sizeof(float));
}

float vis_ease_out_cubic(float t)
{
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

