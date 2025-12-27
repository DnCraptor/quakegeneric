#pragma once
#include <stdint.h>
#include <hardware/dma.h>

#if HDMI_HSTX
#include "data_packet.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// define this for HDMI audio frame sync support
//#define HDMI_AUDIO_FRAMESYNC_MODE_SUPPORT

/*
    timing chart:


    vvvvvvvHVHVvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv    ] vertical sync (1st line may contain HDMI info packets + tail)
    vvvvvvvHVHVvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv    ]
    -------HHHH-----------------------------------------    ] vertical front porch
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
    uint32_t flags;             // polarity etc
};

// dvi_sm_state_t::modeflags
enum {
    DVI_MODE_FLAGS_NO_H_BORDER                  = (1 << 0),
    DVI_MODE_FLAGS_PIXEL_REP                    = (1 << 1),
    DVI_MODE_FLAGS_HDMI_PACKETS                 = (1 << 2),
    DVI_MODE_FLAGS_HDMI_AUDIO                   = (1 << 3),
    DVI_MODE_FLAGS_HDMI_AUDIO_IN_HSYNC          = (1 << 4), // inject audio packets in HSync, not front porch
    DVI_MODE_FLAGS_HDMI_AUDIO_FRAMESYNC           = (1 << 5), // audio frame sync mode
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

#define TMDS_SYNC_V0_H0_WITH_PREAMBLE (TMDS_CTRL_00 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_00 << 20))
#define TMDS_SYNC_V0_H1_WITH_PREAMBLE (TMDS_CTRL_01 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_00 << 20))
#define TMDS_SYNC_V1_H0_WITH_PREAMBLE (TMDS_CTRL_10 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_00 << 20))
#define TMDS_SYNC_V1_H1_WITH_PREAMBLE (TMDS_CTRL_11 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_00 << 20))

#define TMDS_SYNC_V0_H0_WITH_DATA_ISLAND_PREAMBLE (TMDS_CTRL_00 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20))
#define TMDS_SYNC_V0_H1_WITH_DATA_ISLAND_PREAMBLE (TMDS_CTRL_01 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20))
#define TMDS_SYNC_V1_H0_WITH_DATA_ISLAND_PREAMBLE (TMDS_CTRL_10 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20))
#define TMDS_SYNC_V1_H1_WITH_DATA_ISLAND_PREAMBLE (TMDS_CTRL_11 | (TMDS_CTRL_01 << 10) | (TMDS_CTRL_01 << 20))
#define TMDS_VIDEO_LEADING_GUARD_BAND (0x2ccu | (0x133u << 10) | (0x2ccu << 20))

#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)
#define HSTX_CMD_LEN_MASK    (0xFFFu)

#define VGA_HSTX_VSYNC       0x80808080
#define VGA_HSTX_HSYNC       0x40404040

#define VGA_HSTX_SYNC_V0_H0     (0)
#define VGA_HSTX_SYNC_V0_H1     (VGA_HSTX_HSYNC)
#define VGA_HSTX_SYNC_V1_H0     (VGA_HSTX_VSYNC)
#define VGA_HSTX_SYNC_V1_H1     (VGA_HSTX_VSYNC|VGA_HSTX_HSYNC)

#define VGA_HSTX_RGB222 (r, g, b)    (((r) << 4)  | ((g) << 2) | ((b) << 0))
#define VGA_HSTX_RGB222P(r, g, b, p) ((((r) << 4) | ((g) << 2) | ((b) << 0)) << (p*8))

#ifdef HDMI_HSTX

#endif

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

struct __packed hdmi_audio_task_t {
    uint16_t frames_to_render;
    uint16_t write_ptr;
};

#if HDMI_HSTX
typedef int (*hdmi_audio_cb_t) (int16_t *ptr, uint32_t frames, void *priv);
#endif

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
    uint8_t irq_audio_callback;     // exclusive user IRQ line
    uint8_t reserved;
};

// display state machine
struct dvi_sm_state_t {
    // main SM stuff
    uint8_t state;              // current SM state
    uint8_t cmdlist_idx;        // current command list

    struct {
        uint8_t back_porch_end;
        uint8_t active_pixels_end;
        uint16_t back_porch_end_scanlines;
        uint16_t active_pixels_end_scanlines;
    } next_state;               // next states

    struct  {
        uint8_t vsync_off, vsync_on, v_border, v_active;
    } ctrl_len,
#if HDMI_HSTX
    ctrl_pkt_start
#endif
    ;

    uint16_t scanlines_state;    // and how many left in the current state
    uint16_t scanline;           // current scanline (sort of)

    uint32_t frame;              // current frame displayed

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

#if HDMI_HSTX
    struct {
        uint8_t infoframe_len;
        uint8_t infoframe_pre_len;
        uint8_t infoframe_pre_pkt_start;
        uint8_t sync_xormask;
    } hdmi_pkt;

    struct audio_t {
        struct {
            int16_t *ptr;
            uint16_t size;                  // in frames
            uint16_t size_half;
            uint16_t avail;
            uint16_t avail_add;             // a latch to add to the current avail
            uint16_t read_ptr;
            uint8_t  idx;
            uint8_t  framect;           // 
        } buf;
        struct hdmi_audio_task_t task;
        struct {
            hdmi_audio_cb_t  proc;
            void            *priv;
        } cb, cb_latch;
        uint16_t acc, add;  // sample packet accumulator
        struct {
            uint16_t acc, add;
        } acr;
        struct {
            uint16_t count, reload;
        } samples_per_frame;
    } audio;

    uint32_t preamble[2];   // precaculated preambles
#endif

    // resources
    struct dvi_resources_t res;

    // timings struct
    struct dvi_timings_t timings;

    // mode flags
    uint32_t modeflags;

    // ack masks for faster callback IRQ acknowledgment
    struct {
        io_rw_32 *icpr;
        io_rw_32 *ispr;
        uint32_t  mask;
    } cb_irq_ack;
    struct {
        io_rw_32 *icpr;
        io_rw_32 *ispr;
        uint32_t  mask;
    } audio_cb_irq_ack;
    struct {
        io_rw_32 *inte;
        io_rw_32 *ints;
        uint32_t  mask;
    } dma_irq_ack;
};

// state machine phases
enum {
    DVI_STATE_IDLE                      = 0,
    DVI_STATE_ACTIVE_NOP                = 1,  // alignment NOPs

    DVI_STATE_BACK_PORCH                = 2,
    DVI_STATE_TOP_BORDER                = 3,
    DVI_STATE_ACTIVE_BLANK              = 4,
    DVI_STATE_ACTIVE_PIXELS             = 5,
    DVI_STATE_BOTTOM_BORDER             = 6,
    DVI_STATE_FRONT_PORCH               = 7,
    
    DVI_STATE_SYNC_BIT                  = 8,
    DVI_STATE_SYNC                      = 8,
    DVI_STATE_SYNC_HDMI_PACKET          = 9,
    DVI_STATE_SYNC_HDMI_PACKET_PRE      = 10,
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

#if HDMI_HSTX
// init HDMI mode, setup metadata and calculate stuff for audio transmission
// sample_rate=0  - no audio used 
// vic        =-1 - use default VIC from timings table
// modeflags      - only DVI_MODE_FLAGS_HDMI_AUDIO_FREERUN and DVI_MODE_FLAGS_HDMI_AUDIO_VALID_CHANSTATUS are valid
int hdmi_linebuf_init_info(int avi_b1, int avi_b2, int avi_b3, int vic, int sample_rate, int modeflags);

// set buffer and audio watermark (read level below which an audio update is scheduled)
// avail_frames>0 means the buffer is pre-filled with size_written frames of audio
void hdmi_audio_set_buffer(int16_t *buf, int samples_in_buffer);

// set audio buffer callback
void hdmi_audio_set_cb(hdmi_audio_cb_t cb, void *priv);

#endif

// fill HSTX command list
int dvi_linebuf_fill_hstx_cmdlist(int is_hdmi);
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
volatile int dvi_linebuf_get_frame_count();

// get state machine internal structure (DEBUG ONLY!)
struct dvi_sm_state_t* dvi_linebuf_get_sm();

#ifdef __cplusplus
}
#endif