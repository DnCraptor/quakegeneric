#include "dvi_modes.h"

const struct dvi_mode_t dvi_modes[] = {
    // DVI_MODE_320x240
    {
        .name = "320x240",
        .width = 320, .height = 240,
        .pixel_rep = 2, .line_rep = 2,
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
    // DVI_MODE_640x360_LETTERBOX
    {
        .name = "640x360 letterbox",
        .width = 640, .height = 360,
        .pixel_rep = 1, .line_rep = 1,
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
                .active        = 360,
                .border_top    = (480-360)/2,
                .border_bottom = (480-360)/2,
            },
            .refresh       = 60*1000,
            .pixelclock = 25200*1000,
            .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
        }
    }, 
    // DVI_MODE_640x360_LETTERBOX_640x512
    {
        .name = "640x360 letterbox",
        .width = 640, .height = 360,
        .pixel_rep = 1, .line_rep = 1,
        .timings = {
            .h = {
                .front_porch  = 24,
                .sync         = 96,
                .back_porch   = 84,
                .active       = 640,
                .border_left  = (640-640)/2,
                .border_right = (640-640)/2,
            },
            .v = {
                .front_porch   = 1,
                .sync          = 3,
                .back_porch    = 17,
                .active        = 360,
                .border_top    = (512-360)/2,
                .border_bottom = (512-360)/2,
            },
            .refresh       = 60*1000,
            .pixelclock = 27000*1000,
            .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
        }
    }, 
    // DVI_MODE_640x360_1280X720
    {
        .name = "640x360 in 1280x480 timings",
        .width = 1280, .height = 360,
        .pixel_rep = 2, .line_rep = 1,
        .timings = {
            .h = {
                .front_porch  = 16*2,
                .sync         = 96*2,
                .back_porch   = 48*2,
                .active       = 1280,
                .border_left  = (1280-1280)/2,
                .border_right = (1280-1280)/2,
            },
            .v = {
                .front_porch   = 10,
                .sync          = 2,
                .back_porch    = 33,
                .active        = 360,
                .border_top    = (480-360)/2,
                .border_bottom = (480-360)/2,
            },
            .refresh    = 60*1000,
            .pixelclock = 2*25200*1000,
            .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
        }
    },
    // DVI_MODE_640x360_1280x720
    {
        .name = "640x360 in 1280x360 timings",
        .width = 1280, .height = 360,
        .pixel_rep = 2, .line_rep = 1,
        .timings = {
            .h = {
                .front_porch  = 16*2,
                .sync         = 96*2,
                .back_porch   = 48*2,
                .active       = 1280,
                .border_left  = (1280-1280)/2,
                .border_right = (1280-1280)/2,
            },
            .v = {
                .front_porch   = 10+((480-360)/2),
                .sync          = 2,
                .back_porch    = 33+((480-360)/2),
                .active        = 360,
                .border_top    = (360-360)/2,
                .border_bottom = (360-360)/2,
            },
            .refresh    = 60*1000,
            .pixelclock = 2*25200*1000,
            .flags = DVI_TIMINGS_H_NEG | DVI_TIMINGS_V_NEG
        }
    },
    // DVI_MODE_640x360_1280X720
    {
        .name = "640x360 in 1280x720 timings",
        .width = 1280, .height = 720,
        .pixel_rep = 2, .line_rep = 2,
        .timings = {
            .h = {
                .front_porch  = 110,
                .sync         = 40,
                .back_porch   = 220,
                .active       = 1280,
                .border_left  = (1280-1280)/2,
                .border_right = (1280-1280)/2,
            },
            .v = {
                .front_porch   = 5,
                .sync          = 5,
                .back_porch    = 20,
                .active        = 720,
                .border_top    = (720-720)/2,
                .border_bottom = (720-720)/2,
            },
            .refresh    = 60*1000,
            .pixelclock = 74400*1000,
            .flags = DVI_TIMINGS_H_POS | DVI_TIMINGS_V_NEG
        }
    }
};
