// src/visualization/v_engine.c 

#include "v_engine.h"
#include "vis_bars.h"
#include "vis_circles.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// private state, hidden from the header

static int      g_screen_w = 0;
static int      g_screen_h = 0;
static size_t   g_bin_count = 0;

// double buffer so render and update never race
static float    *g_vis_buf[2] = { NULL, NULL };
static int       g_front = 0;

// flags to enable/disable stytles
static bool     g_use_bars      = true;
static bool     g_use_circles   = true;
