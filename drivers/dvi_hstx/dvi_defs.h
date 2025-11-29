#pragma once
#include <stdint.h>
#include <hardware/dma.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    timing chart:


    vvvvvvvHVHVvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv    ] vertical sync 
    vvvvvvvHVHVvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv    ]
    -------HHHH-----------------------------------------    ] vertical front porch (w/ optional HDMI data islands)
    -------HHHH-----------------------------------------    ]
    b------HHHH----bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb    ] vertical border (optional)
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ] active area
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA    ]
    b------HHHH----bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb    ] vertical border (optional)
    -------hhhh-----------------------------------------    ] vertical back porch
    -------hhhh-----------------------------------------    ] 
    ^ ^     ^    ^  ^  active or vertical border area
    ^ ^     ^    front porch
    ^ ^     sync   ^
    ^ back porch   ^
    ^              ^
    horizontal border (optional)
*/

// HSTX pin layout struct, pin numbers relative to GP12
union dvi_hstx_pin_layout_t {
    struct {
        uint32_t clock_n : 4;
        uint32_t clock_p : 4;
        uint32_t lane0_n : 4;
        uint32_t lane0_p : 4;
        uint32_t lane1_n : 4;
        uint32_t lane1_p : 4;
        uint32_t lane2_n : 4;
        uint32_t lane2_p : 4;
    };
    struct {
        uint32_t vga_b0  : 4;
        uint32_t vga_b1  : 4;
        uint32_t vga_g0  : 4;
        uint32_t vga_g1  : 4;
        uint32_t vga_r0  : 4;
        uint32_t vga_r1  : 4;
        uint32_t vga_hs  : 4;
        uint32_t vga_vs  : 4;
    };
    uint32_t raw;
};

// DVI/HDMI timing struct
struct dvi_timings_t {
    // length of each frame part
    struct {
        int16_t front_porch;
        int16_t sync;
        int16_t back_porch;
        int16_t active;
        int16_t border_left;
        int16_t border_right;
        int16_t total;
    } h;

    struct {
        int16_t front_porch;
        int16_t sync;
        int16_t active;
        int16_t back_porch;
        int16_t border_top;
        int16_t border_bottom;
        int16_t total;
    } v;

    union {
        struct {
            int16_t active;
            int16_t border_left;
            int16_t border_right;
            int16_t reserved;
        } pixelrep;
        struct {
            int32_t  refresh;     // in ([hz]*1000), 0 if calculate from current hstx_clk + timing parameters
            int32_t  pixelclock;  // requested pixel clock in Hz, 0 if use (current_hstx_clk/5)
        };
    };
    uint32_t flags;             // polarity etc
};

enum {
    // sync polarity flags
    DVI_TIMINGS_H_POS = (0 << 0),
    DVI_TIMINGS_H_NEG = (1 << 0),
    DVI_TIMINGS_V_POS = (0 << 1),
    DVI_TIMINGS_V_NEG = (1 << 1),
    DVI_TIMINGS_SYNC_POLARITY_MASK = (3 << 0),
};

enum {
    HSTX_TIMINGS_REFRESH_VBLANK = (0 << 0),  // stretch vblank to change refresh rate
    HSTX_TIMINGS_REFRESH_HBLANK = (1 << 0),  // stretch hblank instead (more compatible with HDMI->VGA?)
    HSTX_TIMINGS_VGA_FIXUP      = (1 << 1),  // treat mode as VGA (divide all values by pixel_rep)
};

// ----------------------------------------------------------------------------
// DVI constants (TODO: HDMI)

#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define TMDS_SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define TMDS_SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define TMDS_SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define TMDS_SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

#define VGA_HSTX_VSYNC       0x80808080
#define VGA_HSTX_HSYNC       0x40404040

#define VGA_HSTX_SYNC_V0_H0     (0)
#define VGA_HSTX_SYNC_V0_H1     (VGA_HSTX_HSYNC)
#define VGA_HSTX_SYNC_V1_H0     (VGA_HSTX_VSYNC)
#define VGA_HSTX_SYNC_V1_H1     (VGA_HSTX_VSYNC|VGA_HSTX_HSYNC)

#define VGA_HSTX_RGB222 (r, g, b)    (((r) << 4)  | ((g) << 2) | ((b) << 0))
#define VGA_HSTX_RGB222P(r, g, b, p) ((((r) << 4) | ((g) << 2) | ((b) << 0)) << (p*8))

// DMA channel used
enum {
    DVI_DMACH_SRAM_HSTX,
    DVI_DMACH_DISPLAYLIST,
    DVI_DMACH_DISPLAYLIST_RESET,

    DVI_DMACH_COUNT
};

struct dvi_dma_cmd_list {
    uintptr_t read_addr;
    uintptr_t write_addr;
    uint32_t transfer_count;
    uint32_t ctrl_trig;
};

// -------------------

// HSTX pixel mode definitions
enum {
    DVI_HSTX_MODE_MONO1_LSB,        // 1bpp monochrome, LSB first
    DVI_HSTX_MODE_XRGB8888,         // XRGB8888
    VGA_HSTX_MODE_PWM32,            // VGA PWM mode

    DVI_HSTX_MODE_COUNT
};

// -------------------
// forward decl
struct dvi_sm_state_t;

enum {
    DVI_CB_STATE_IDLE       = 0,
    DVI_CB_STATE_REQUEST    = (1 << 1),
    DVI_CB_STATE_NEW_FRAME  = (1 << 2),
    DVI_CB_STATE_INSIDE     = (1 << 3)      // callback is the owner
};

struct __packed dvi_linebuf_task_t {
    void     *dst;
    uint16_t  state;
    uint16_t  line;                 // current line displaying
    uint16_t  width, height;        // width and height of rendering area
    uint16_t  pitch, pitch_fixup;   // distance between successive lines in bytes and fixup
};

// line buffer callback signature
typedef void (*dvi_linebuf_cb_t)(const struct dvi_linebuf_task_t *task, void *priv);

// -------------------

// command list entry
typedef struct {
    uint32_t read_addr;
    uint32_t write_addr;
    uint32_t transfer_count;
    uint32_t ctrl_trig;
} dvi_dma_cmdlist;

// required DVI resources for the line buffer mode
struct dvi_resources_t {
    uint8_t dma_channels[3];
    uint8_t irq_dma;                // exclusive DMA  IRQ line
    uint8_t irq_linebuf_callback;   // exclusive user IRQ line
};

// display state machine
struct dvi_sm_state_t {
    // main SM stuff
    uint8_t state;              // current SM state
    uint8_t cmdlist_idx;        // current command list

    struct {
        uint8_t back_porch_end;
        uint8_t active_pixels_end;
        int16_t back_porch_end_scanlines;
        int16_t active_pixels_end_scanlines;
    } next_state;               // next states

    struct {
        uint8_t vsync_off, vsync_on, v_border, v_active;
    } ctrl_len;

    int16_t scanline;           // current scanline (sort of)
    int16_t scanlines_state;    // and how many left in the current state

    int32_t frame;              // current frame displayed

    // line repeat stuff
    struct {
        uint16_t count, reload;
    } linerep;

    // line buffer stuff
    struct {
        struct {
            uint32_t pos;
            uint8_t  idx;
            uint8_t  lines;
        } disp;
        struct {
            uint8_t  idx;
            uint8_t  cb_idx;
        } render;

        struct dvi_linebuf_task_t task;
        struct {
            dvi_linebuf_cb_t    proc;
            void               *priv;
        } cb, cb_latch;
    } linebuf;

    // resources
    struct dvi_resources_t res;

    // timings struct
    struct dvi_timings_t timings;

    struct {
        // NVIC offsets for faster callback IRQ acknowledgment
        io_rw_32 *icpr;
        uint32_t  mask;
        io_rw_32 *ispr;
    } cb_irq_ack;
    struct {
        io_rw_32 *irq_ctrl;
        uint32_t  mask;
    } dma_irq_ack;
};

// state machine phases
enum {
    DVI_STATE_IDLE = 0,
    DVI_STATE_SYNC,
    DVI_STATE_BACK_PORCH,
    DVI_STATE_TOP_BORDER,
    DVI_STATE_ACTIVE_NOP,           // dist alignment NOPs
    DVI_STATE_ACTIVE_BLANK,
    DVI_STATE_ACTIVE_PIXELS,
    DVI_STATE_BOTTOM_BORDER,
    DVI_STATE_FRONT_PORCH,
    DVI_STATE_FRONT_PORCH_NOP,      // end of frame alignment NOPs
};

// --------------------------
// general stuff

// adjust DVI timings
int dvi_adjust_timings(struct dvi_timings_t *timings, uint32_t hstx_mode, int pixel_rep, uint32_t flags);

// --------------------------
// general HSTX init

// configure HSTX output pins
void dvi_configure_hstx_output(union dvi_hstx_pin_layout_t layout, uint32_t slew_rate, uint32_t drive_strength); 
#if VGA_HSTX
void vga_configure_hstx_output(union dvi_hstx_pin_layout_t layout, uint32_t slew_rate, uint32_t drive_strength, uint32_t phase_repeats);
#endif

// set command expander mode (pixel mode is always XRGB8888)
int dvi_configure_hstx_command_expander(int hstx_mode, int pix_rep);

// --------------------------
// line buffer mode

// reset DVI line buffer stuff
int dvi_linebuf_reset();

// get memory required for the mode
void dvi_linebuf_get_memsize(struct dvi_timings_t *timings, uint32_t *linebuf_memsize, int pixel_rep);

// set timings
int dvi_linebuf_set_timings(const struct dvi_timings_t *timings, int pix_rep);
int vga_linebuf_set_timings(const struct dvi_timings_t *timings);

// set resources
int dvi_linebuf_set_resources(struct dvi_resources_t *resources, uint32_t *linebuf);

// fill HSTX command list
int dvi_linebuf_fill_hstx_cmdlist();
#if VGA_HSTX
int vga_linebuf_fill_hstx_cmdlist();
#endif

// initialize DMA channels and IRQ handlers
int dvi_linebuf_init_dma();

// set line repetition factor (affects line buffer rendering as well!)
void dvi_linebuf_set_line_rep(int line_rep);

// set line buffer render callback
void dvi_linebuf_set_cb(dvi_linebuf_cb_t cb, void *priv);

// start DVI line buffer output
int dvi_linebuf_start();

// stop DVI line buffer output
int dvi_linebuf_stop();

// deinitialize stuff (memory and DMA/IRQ has to be unclaimed manually)
int dvi_linebuf_done();

// -------
// getters

#if VGA_HSTX
uint32_t vga_pwm_get_pixel_sync_mask();

// translate palette index (VGA PWM mode only)
// must be run after mode init!
uint32_t vga_pwm_xlat_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t vga_pwm_xlat_color32(uint32_t idx);

// translate whole palette, components are assumed in b->g->r order
void     vga_pwm_xlat_palette(uint32_t *dst, uint8_t *src, uint32_t colors, uint32_t src_pitch);
#endif

// get current state
volatile int dvi_linebuf_get_state();

// get frame count
int dvi_linebuf_get_frame_count();

#ifdef __cplusplus
}
#endif