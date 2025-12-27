#pragma once
#include <stdint.h>
#include "dvi_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dvi_mode_t {
    const char *name;
    uint16_t width, height;
    uint8_t  pixel_rep;     // horizontal replication
    uint8_t  line_rep;      // vertical   replication
    uint8_t  vic;           // HDMI Video Information Code
    uint8_t  cea_aspect;    // CEA-861 aspect ratio info
    struct dvi_timings_t timings;
};

extern const struct dvi_mode_t dvi_modes[];

// list of modes
enum {
    DVI_MODE_320x240,
    DVI_MODE_360x240,
    DVI_MODE_400x300,
    DVI_MODE_640x480,
    DVI_MODE_COUNT
};

#ifdef __cplusplus
}
#endif
