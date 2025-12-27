// VGA/DVI/HDMI HSTX display driver with audio support for the RP2350 - work in progress
// --wbcbz7 28.12.2o25

// SPDX-License-Identifier: MIT

#include "dvi_modes.h"

const struct dvi_mode_t dvi_modes[] = {
    // DVI_MODE_320x240
    {
        .name = "320x240",
        .width = 320, .height = 240,
        .pixel_rep = 2, .line_rep = 2,
        .vic = VIC_640x480p_60hz,
        .cea_aspect = AVI_ASPECT_RATIO_4_3 | AVI_ACTIVE_FORMAT_SAME,
        .timings = {
            .h = {
                .front_porch  = 16,
                .sync         = 96,
                .back_porch   = 48,
                .active       = 640,
                .border_left  = (640-640)/2,
                .border_right = (640-640)/2,
            },
            .v = {
                .front_porch   = 10,
                .sync          = 2,
                .back_porch    = 33,
                .active        = 480,
                .border_top    = (480-480)/2,
                .border_bottom = (480-480)/2,
            },
            .refresh       = 60*1000,
            .pixelclock = 25200*1000,
            .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
        }
    }, 
    // DVI_MODE_640x480
    {
        .name = "640x480",
        .width = 640, .height = 480,
        .pixel_rep = 1, .line_rep = 1,
        .vic = VIC_640x480p_60hz,
        .cea_aspect = AVI_ASPECT_RATIO_4_3 | AVI_ACTIVE_FORMAT_SAME,
        .timings = {
            .h = {
                .front_porch  = 16,
                .sync         = 96,
                .back_porch   = 48,
                .active       = 640,
                .border_left  = (640-640)/2,
                .border_right = (640-640)/2,
            },
            .v = {
                .front_porch   = 10,
                .sync          = 2,
                .back_porch    = 33,
                .active        = 480,
                .border_top    = (480-480)/2,
                .border_bottom = (480-480)/2,
            },
            .refresh       = 60*1000,
            .pixelclock = 25200*1000,
            .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
        }
    },
};
