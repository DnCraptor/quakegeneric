// VGA/DVI/HDMI HSTX display driver with audio support for the RP2350 - work in progress
// --wbcbz7 28.12.2o25

// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/sio.h"
#include "dvi_defs.h"

// line buffer static stuff
#if HDMI_HSTX

#define HDMI_AUDIO_CTRL_COUNT 4
#define HDMI_AUDIO_PACKETS_PER_LINE 1       // set in single data island
#define HDMI_AUDIO_PACKET_LEN (W_PREAMBLE + 2*W_GUARDBAND + W_DATA_PACKET*HDMI_AUDIO_PACKETS_PER_LINE)

// control words
static uint32_t dvi_hstx_ctrl_nop[1];
static uint32_t dvi_hstx_ctrl_vsync_off[9];
static uint32_t dvi_hstx_ctrl_vsync_on[9];
static uint32_t dvi_hstx_ctrl_infoframe[144];
static uint32_t dvi_hstx_ctrl_infoframe_pre[8];
static uint32_t dvi_hstx_ctrl_v_border[14]; // incl. blanks if present
static uint32_t dvi_hstx_ctrl_v_active[15]; // incl. blanks if present
static uint32_t dvi_hstx_ctrl_audio_packet[HDMI_AUDIO_CTRL_COUNT][64]; // probably enough :p
#else
// control words
static uint32_t dvi_hstx_ctrl_nop[1];
static uint32_t dvi_hstx_ctrl_vsync_off[9];
static uint32_t dvi_hstx_ctrl_vsync_on[9];
static uint32_t dvi_hstx_ctrl_v_border[10];
static uint32_t dvi_hstx_ctrl_v_active[11]; // incl. blanks if present
#endif

// --------------------------------
// command list
#define COMMAND_LIST_LENGTH 2
#define COMMAND_LIST_COUNT  2

// command list itself and read address pointers
static struct dvi_dma_cmd_list dma_cmdlist[COMMAND_LIST_LENGTH*COMMAND_LIST_COUNT];
static __not_in_flash("dvi") __aligned(8) uintptr_t dma_cmdlist_read_addr[] = {
    (uintptr_t)&dma_cmdlist[COMMAND_LIST_LENGTH*1],
    (uintptr_t)&dma_cmdlist[COMMAND_LIST_LENGTH*0],
};

// big static state structure
static struct dvi_sm_state_t dvi_sm;

// --------------------------------
// line buffer
#define LINES_PER_BUFFER 1
#define LINE_BUFFER_COUNT 3
static uint32_t *dvi_linebuf[LINE_BUFFER_COUNT];       // preallocated

// TMDS sync words
enum {
    SYNC_WORD_DATA_PREAMBLE_OFFSET = 4,
    SYNC_WORD_PREAMBLE_OFFSET = 8,
};

static const uint32_t __not_in_flash("dvi.const") tmds_sync_word[12] = {
    TMDS_SYNC_V0_H0,
    TMDS_SYNC_V0_H1,
    TMDS_SYNC_V1_H0,
    TMDS_SYNC_V1_H1,
    TMDS_SYNC_V0_H0_WITH_DATA_ISLAND_PREAMBLE,
    TMDS_SYNC_V0_H1_WITH_DATA_ISLAND_PREAMBLE,
    TMDS_SYNC_V1_H0_WITH_DATA_ISLAND_PREAMBLE,
    TMDS_SYNC_V1_H1_WITH_DATA_ISLAND_PREAMBLE,
    TMDS_SYNC_V0_H0_WITH_PREAMBLE,
    TMDS_SYNC_V0_H1_WITH_PREAMBLE,
    TMDS_SYNC_V1_H0_WITH_PREAMBLE,
    TMDS_SYNC_V1_H1_WITH_PREAMBLE
};

// VGA sync words
static const uint32_t vga_sync_word[4] = {
    VGA_HSTX_SYNC_V1_H1, VGA_HSTX_SYNC_V1_H0, VGA_HSTX_SYNC_V0_H1, VGA_HSTX_SYNC_V0_H0
};

// --------------------------
// general HSTX init

// configure HSTX output pins
void dvi_configure_hstx_output(union dvi_hstx_pin_layout_t layout, uint32_t slew_rate, uint32_t drive_strength) {
    // Serial output config: clock period of 5 cycles, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // assign pins
    hstx_ctrl_hw->bit[layout.clock_n] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.clock_p] = HSTX_CTRL_BIT0_CLK_BITS;

    hstx_ctrl_hw->bit[layout.lane0_n] = ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane0_p] = ((0*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[layout.lane1_n] = ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane1_p] = ((1*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[layout.lane2_n] = ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB) | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[layout.lane2_p] = ((2*10 + 0) << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2*10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB);

    // set pin function to HSTX
    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, GPIO_FUNC_HSTX); // HSTX
        gpio_set_slew_rate(i, slew_rate);
        gpio_set_drive_strength(i, drive_strength);
    }
}

// configure HSTX output pins
void vga_configure_hstx_output(union dvi_hstx_pin_layout_t layout, uint32_t slew_rate, uint32_t drive_strength, uint32_t phase_repeats) {
    // Serial output config: clock period of 4 cycles, pop from command expander every 4 cycles
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        4u << HSTX_CTRL_CSR_CLKDIV_LSB |
        phase_repeats << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        16u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // assign pins
    hstx_ctrl_hw->bit[layout.vga_vs] = (7 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((7+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // vsync
    hstx_ctrl_hw->bit[layout.vga_hs] = (6 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((6+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // hsync
    hstx_ctrl_hw->bit[layout.vga_r1] = (5 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((5+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // R
    hstx_ctrl_hw->bit[layout.vga_r0] = (4 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((4+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // r
    hstx_ctrl_hw->bit[layout.vga_g1] = (3 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((3+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // G
    hstx_ctrl_hw->bit[layout.vga_g0] = (2 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((2+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // g
    hstx_ctrl_hw->bit[layout.vga_b1] = (1 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((1+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // B
    hstx_ctrl_hw->bit[layout.vga_b0] = (0 << HSTX_CTRL_BIT0_SEL_P_LSB) | ((0+8) << HSTX_CTRL_BIT0_SEL_N_LSB); // b

    // set pin function to HSTX
    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, GPIO_FUNC_HSTX); // HSTX
        gpio_set_slew_rate(i, slew_rate);
        gpio_set_drive_strength(i, drive_strength);
    }
}

struct dvi_mode_props_t {
    uint32_t hstx_expand;
    uint8_t  pixels_per_word;
    uint8_t  bits_per_pixels;
    uint8_t  flags;
    uint8_t  reserved;
};

static struct dvi_mode_props_t dvi_mode_props[DVI_HSTX_MODE_COUNT] = {
    // DVI_HSTX_MODE_MONO1_LSB
    {
        .hstx_expand = (
            0 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            ((0-7)&31) << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            0 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            ((0-7)&31) << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            0 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            ((0-7)&31) << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB
        ),
        .pixels_per_word = 32,
        .bits_per_pixels = 1,
        .flags = DVI_MODE_FLAGS_NO_H_BORDER
    },
    // DVI_HSTX_MODE_XRGB8888
    {
        .hstx_expand = (
            7  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            16 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            7  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            8  << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            7  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            0  << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB
        ),
        .pixels_per_word = 1,
        .bits_per_pixels = 32,
        .flags = DVI_MODE_FLAGS_PIXEL_REP
    },
    // VGA_HSTX_MODE_PWM32
    {
        .hstx_expand = 0,           // don't care as raw pins mode is used
        .pixels_per_word = 1,
        .bits_per_pixels = 32,
        .flags = DVI_MODE_FLAGS_PIXEL_REP
    },
};

// set command expander mode
int dvi_configure_hstx_command_expander(int mode, int pix_rep) {
    hstx_ctrl_hw->expand_tmds = dvi_mode_props[mode].hstx_expand;
    if (dvi_mode_props[mode].flags & DVI_MODE_FLAGS_PIXEL_REP) {
        hstx_ctrl_hw->expand_shift = 
                (pix_rep) << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
                0 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
                1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
                0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
    } else {
        hstx_ctrl_hw->expand_shift = 
                (dvi_mode_props[mode].pixels_per_word&31)  << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
                (dvi_mode_props[mode].bits_per_pixels&31)  << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
                1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
                0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
    }

    return 0;
}

// adjust timings
int dvi_adjust_timings(struct dvi_timings_t *timings, uint32_t hstx_mode, int pixel_rep, uint32_t flags) {
    if (timings == NULL) return 1;

    // intermediate horizontal/vertical total registers
    uint32_t htotal, vtotal;

    // round horizontal active and borders area to pixel repetition factor
    htotal = timings->h.active+timings->h.border_left+timings->h.border_right;
    int pixel_rep_rem = htotal % pixel_rep;
    if (pixel_rep_rem != 0) {
        printf("h_active+border_left+border_right not divisible by pixel_rep!\n");
        return 1;
    }

    if (flags & HSTX_TIMINGS_VGA_FIXUP) {
        // divide __all__ horizontal timing values by pixel_rep
        timings->h.border_right /= pixel_rep;
        timings->h.back_porch   /= pixel_rep;
        timings->h.sync         /= pixel_rep;
        timings->h.front_porch  /= pixel_rep;
        timings->h.border_left  /= pixel_rep;
        timings->h.active       /= pixel_rep;
    }

    // remove horizontal borders from the 1bpp mono modes because we need them to be aligned
    // by 32 pixels, and this is very tedious :/
    if (hstx_mode == DVI_HSTX_MODE_MONO1_LSB) {
        timings->h.active += timings->h.border_left + timings->h.border_right;
        timings->h.border_left = timings->h.border_right = 0;
    }

    if (timings->pixelclock != 0) {
        // user - defined pixel clock
        timings->h.total = 
            // this order reflects the order of command words in HSTX
            timings->h.border_right+
            timings->h.back_porch+
            timings->h.sync+
            timings->h.front_porch+
            timings->h.border_left+
            timings->h.active;
        timings->v.total = 
            timings->v.sync+
            timings->v.front_porch+
            timings->v.border_top+
            timings->v.active+
            timings->v.border_bottom+
            timings->v.back_porch;

        // calculate refresh rate
        timings->refresh = ((timings->pixelclock * 1000LL) / (timings->h.total * timings->v.total));
    } else {
        // TODO: recalculate pixel clock from desired refresh rate
        return 1;
    }

    // save mode flags
    dvi_sm.modeflags = flags;
    return 0;
}

// ------------------------------
// very hacky inline version of irq_set_pending()
// (the pico-sdk one generates a call to flash. in an ISR. awesome :)
static inline void dvi_irq_set_pending(uint num) {
    *dvi_sm.cb_irq_ack.ispr = dvi_sm.cb_irq_ack.mask;
}
static inline void hdmi_audio_irq_set_pending(uint num) {
    *dvi_sm.audio_cb_irq_ack.ispr = dvi_sm.audio_cb_irq_ack.mask;
}

void __not_in_flash_func(hdmi_inject_audio_packet)(struct dvi_sm_state_t *ctx, struct dvi_dma_cmd_list *dl, uint32_t read_addr, uint32_t transfer_count, int32_t pkt_start, int samples_in_frame) {
    data_packet_t pkt;
    int16_t tempsamples[4*2];

    if (pkt_start < 0) return;

    if (ctx->audio.buf.avail_add != 0) {
        ctx->audio.buf.avail      += ctx->audio.buf.avail_add;
        ctx->audio.task.write_ptr += ctx->audio.buf.avail_add;
        if (ctx->audio.task.write_ptr >= ctx->audio.buf.size) {
            ctx->audio.task.write_ptr -= ctx->audio.buf.size;
        }
        ctx->audio.buf.avail_add = 0;
    }
    int samples_to_copy = MIN(MIN(samples_in_frame, ctx->audio.buf.avail), 4);
    if (samples_to_copy > 0) { 
#ifdef HDMI_AUDIO_FRAMESYNC_MODE_SUPPORT
        if ((ctx->audio.buf.read_ptr + samples_to_copy <= ctx->audio.buf.size)) {
            // all ok, read and build data packet
            ctx->audio.buf.framect = hdmi_audio_set_audio_sample(
                &pkt,
                ctx->audio.buf.ptr + ctx->audio.buf.read_ptr*2,
                samples_to_copy,
                ctx->audio.buf.framect
            );
        } else {
            // handle buffer boundary
            int16_t* tbuf=tempsamples, *sbuf = ctx->audio.buf.ptr + ctx->audio.buf.read_ptr*2;
            int left = ctx->audio.buf.size - ctx->audio.buf.read_ptr;
            {int i = left; if (i > 0) do {*tbuf++=*sbuf++;*tbuf++=*sbuf++;} while (--i);}
            sbuf = ctx->audio.buf.ptr;
            {int i = samples_to_copy-left; if (i > 0) do {*tbuf++=*sbuf++;*tbuf++=*sbuf++;} while (--i);}
            // output temp packet
            ctx->audio.buf.framect = hdmi_audio_set_audio_sample(
                &pkt,
                &tempsamples[0],
                samples_to_copy,
                ctx->audio.buf.framect
            );
        }
#else
        samples_to_copy = 4;
        // all ok, read and build data packet
        ctx->audio.buf.framect = hdmi_audio_set_audio_sample(
            &pkt,
            ctx->audio.buf.ptr + ctx->audio.buf.read_ptr*2,
            samples_to_copy,
            ctx->audio.buf.framect
        );
#endif
        ctx->audio.buf.read_ptr += samples_to_copy;
        if (ctx->audio.buf.read_ptr >= ctx->audio.buf.size) {
            ctx->audio.buf.read_ptr -= ctx->audio.buf.size;
        }
        ctx->audio.buf.avail -= samples_to_copy;
    } else {
        // output silent packet
        memset(tempsamples, 0, sizeof(tempsamples));
        ctx->audio.buf.framect = hdmi_audio_set_audio_sample(
            &pkt,
            &tempsamples[0],
            samples_to_copy,
            ctx->audio.buf.framect
        );
    }

    if (ctx->audio.buf.avail <= ctx->audio.buf.size_half && ctx->audio.buf.avail_add == 0) {
        // post new task and call callback
        ctx->audio.task.frames_to_render = ctx->audio.buf.size_half;
        hdmi_audio_irq_set_pending(ctx->res.irq_audio_callback);
    }

    uint32_t *ctrl_word     = dvi_hstx_ctrl_audio_packet[ctx->audio.buf.idx];
    uint32_t *ctrl_word_cur = ctrl_word;
    uint32_t *p = (uint32_t*)read_addr;
    uint32_t syncflags = (ctx->timings.flags & DVI_TIMINGS_SYNC_POLARITY_MASK) ^ ctx->hdmi_pkt.sync_xormask ^ (ctx->state & DVI_STATE_SYNC_BIT ? 0 : DVI_TIMINGS_V_NEG);

    // copy first words from source buffer
    { int i = pkt_start; while (i) do {*ctrl_word_cur++ = *p++;} while (--i); }

    // inject HDMI audio packet
    *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (W_PREAMBLE);
    *ctrl_word_cur++ = dvi_sm.preamble[ctx->state & DVI_STATE_SYNC_BIT ? 0 : 1];
    int pkt_words = encode_data_islands_hstx(ctrl_word_cur+1, &pkt, 1, syncflags);
    *ctrl_word_cur = HSTX_CMD_RAW | pkt_words;
    ctrl_word_cur += (pkt_words+1);

    // copy and fixup trailing control data
    *ctrl_word_cur++ = (*p & ~HSTX_CMD_LEN_MASK) | ((*p & HSTX_CMD_LEN_MASK) - pkt_words - W_PREAMBLE); p++;
    { int i = transfer_count - pkt_start - 1; while (i) do {*ctrl_word_cur++ = *p++;} while (--i); }

    // and write to the command list
    dl->read_addr = (uintptr_t)ctrl_word;
    dl->transfer_count = ctrl_word_cur - ctrl_word;

    ctx->audio.buf.idx = (ctx->audio.buf.idx + 1) & (HDMI_AUDIO_CTRL_COUNT-1);
}

#if HDMI_HSTX
// fill dma command list entry, advance DVI state
static void __not_in_flash_func(dvi_state_advance)(struct dvi_sm_state_t *ctx, struct dvi_dma_cmd_list *dl, int dlidx) {
    uint32_t read_addr, transfer_count; int pkt_start = -1, next_state = ctx->state;
    switch (ctx->state) {
        case DVI_STATE_IDLE:
            break;
        case DVI_STATE_SYNC_HDMI_PACKET_PRE:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_infoframe_pre;
            transfer_count = ctx->hdmi_pkt.infoframe_pre_len;
            pkt_start      = ctx->hdmi_pkt.infoframe_pre_pkt_start;
            next_state = DVI_STATE_SYNC_HDMI_PACKET;
#ifdef HDMI_AUDIO_FRAMESYNC_MODE_SUPPORT
            if (ctx->modeflags & DVI_MODE_FLAGS_HDMI_AUDIO_FRAMESYNC) {
                // reset delta counter and samples in current frame count each frame 
                // for consistent audio scheduling
                ctx->audio.acc = dvi_sm.audio.add;  // rounding fixup
                ctx->audio.samples_per_frame.count = ctx->audio.samples_per_frame.reload;
            }
#endif
            break;
        case DVI_STATE_SYNC_HDMI_PACKET:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_infoframe;
            transfer_count = ctx->hdmi_pkt.infoframe_len;
            next_state = DVI_STATE_SYNC;
            goto sync_advance_scanline_count;
        case DVI_STATE_SYNC:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_vsync_on;
            transfer_count = ctx->ctrl_len.vsync_on;
            pkt_start      = ctx->ctrl_pkt_start.vsync_on;
sync_advance_scanline_count:
            if (--ctx->scanlines_state == 0) {
                // advance
                ctx->scanlines_state = ctx->timings.v.back_porch;

                ctx->linebuf.disp.pos   = 0;
                ctx->linebuf.disp.idx   = 0;
                ctx->linebuf.disp.lines = LINES_PER_BUFFER;
                ctx->linebuf.render.idx = 1;    // used for further sequencing, see below

                ctx->linerep.count = ctx->linerep.reload;

                // update linerep callback
                ctx->linebuf.cb.proc = ctx->linebuf.cb_latch.proc;
                ctx->linebuf.cb.priv = ctx->linebuf.cb_latch.priv;

                ctx->audio.cb.proc = ctx->audio.cb_latch.proc;
                ctx->audio.cb.priv = ctx->audio.cb_latch.priv;

                // fill first two line buffers
                if (ctx->linebuf.task.state == DVI_CB_STATE_IDLE) {
                    ctx->linebuf.task.state  = DVI_CB_STATE_REQUEST | DVI_CB_STATE_NEW_FRAME;
                    ctx->linebuf.task.dst    = dvi_linebuf[0];
                    ctx->linebuf.task.width  = ctx->timings.pixelrep.active;
                    ctx->linebuf.task.height = LINES_PER_BUFFER*2;
                    ctx->linebuf.task.line   = 0;
                    ctx->linebuf.task.pitch        = ctx->timings.pixelrep.active * sizeof(uint32_t);
                    ctx->linebuf.task.pitch_fixup  = ctx->linebuf.task.pitch - (ctx->timings.pixelrep.active * sizeof(uint32_t));
                    dvi_irq_set_pending(dvi_sm.res.irq_linebuf_callback);
                }
                next_state = DVI_STATE_BACK_PORCH;
            }
            break;
        case DVI_STATE_BACK_PORCH:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_vsync_off;
            transfer_count = ctx->ctrl_len.vsync_off;
            pkt_start      = ctx->ctrl_pkt_start.vsync_off;
            if (--ctx->scanlines_state == 0) {
                // advance
                next_state = ctx->next_state.back_porch_end;
                ctx->scanlines_state = ctx->next_state.back_porch_end_scanlines;
            }
            break;
        case DVI_STATE_TOP_BORDER:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_v_border;
            transfer_count = ctx->ctrl_len.v_border;
            pkt_start      = ctx->ctrl_pkt_start.v_border;
            if (--ctx->scanlines_state == 0) {
                // advance
                next_state = DVI_STATE_ACTIVE_NOP;
                ctx->scanlines_state = ctx->timings.v.active;
            }
            break;
        case DVI_STATE_ACTIVE_NOP:
            if (dlidx == 0 || dlidx == COMMAND_LIST_LENGTH-1) next_state = DVI_STATE_ACTIVE_BLANK;
            if (dlidx != 0) {
                read_addr      = (uintptr_t)dvi_hstx_ctrl_nop;
                transfer_count = count_of(dvi_hstx_ctrl_nop);
                break;
            } // fallthrough if dlidx == 0
        case DVI_STATE_ACTIVE_BLANK:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_v_active;
            transfer_count = ctx->ctrl_len.v_active;
            pkt_start      = ctx->ctrl_pkt_start.v_active;
            next_state = DVI_STATE_ACTIVE_PIXELS;
            break;
        case DVI_STATE_ACTIVE_PIXELS:
            read_addr      = (uintptr_t)(dvi_linebuf[ctx->linebuf.disp.idx] + ctx->linebuf.disp.pos);
            transfer_count = ctx->timings.pixelrep.active;
            
            if (--ctx->linerep.count == 0) {
                ctx->linerep.count = ctx->linerep.reload;
                ctx->linebuf.disp.pos += ctx->timings.pixelrep.active;

                // TODO: all the stuff (i'm lazy =)
                if (--ctx->linebuf.disp.lines == 0) {
                    ctx->linebuf.disp.pos   = 0;
                    ctx->linebuf.disp.idx++; if (ctx->linebuf.disp.idx >= LINE_BUFFER_COUNT) ctx->linebuf.disp.idx = 0;
                    ctx->linebuf.disp.lines = LINES_PER_BUFFER;
                    ctx->linebuf.render.idx++; if (ctx->linebuf.render.idx >= LINE_BUFFER_COUNT) ctx->linebuf.render.idx = 0;

                    // fill next buffer
                    if (ctx->linebuf.task.state == DVI_CB_STATE_IDLE) {
                        ctx->linebuf.task.state  = DVI_CB_STATE_REQUEST;
                        ctx->linebuf.task.dst    = dvi_linebuf[ctx->linebuf.render.idx];
                        ctx->linebuf.task.height = LINES_PER_BUFFER;
                        dvi_irq_set_pending(dvi_sm.res.irq_linebuf_callback);
                    }
                }
            }

            if (--ctx->scanlines_state == 0) {
                // advance
                next_state = ctx->next_state.active_pixels_end;
                ctx->scanlines_state = ctx->next_state.active_pixels_end_scanlines;
            } else {
                next_state = DVI_STATE_ACTIVE_BLANK;
            }
            break;
        case DVI_STATE_BOTTOM_BORDER:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_v_border;
            transfer_count = ctx->ctrl_len.v_border;
            pkt_start      = ctx->ctrl_pkt_start.v_border;
            if (--ctx->scanlines_state == 0) {
                // advance
                next_state = DVI_STATE_FRONT_PORCH;
                ctx->scanlines_state = ctx->timings.v.front_porch;
            }
            break;
        case DVI_STATE_FRONT_PORCH:
            read_addr      = (uintptr_t)dvi_hstx_ctrl_vsync_off;
            transfer_count = ctx->ctrl_len.vsync_off;
            pkt_start      = ctx->ctrl_pkt_start.vsync_off;
            if (--ctx->scanlines_state == 0) {
                // next frame
                // TODO: add alignment NOP for odd linecount
                ctx->frame++;
                ctx->scanline = 0;
                next_state = ctx->modeflags & DVI_MODE_FLAGS_HDMI_PACKETS ? DVI_STATE_SYNC_HDMI_PACKET_PRE : DVI_STATE_SYNC;
                ctx->scanlines_state = ctx->timings.v.sync;
            }
            break;
    }

    dl->read_addr       = (uintptr_t)read_addr;
    dl->transfer_count  = transfer_count;
    if ((ctx->modeflags & DVI_MODE_FLAGS_HDMI_AUDIO) && (pkt_start != -1)) {
        // run scheduler
        int acc = (ctx->audio.acc + ctx->audio.add);
        ctx->audio.acc = acc;
        if (acc >= (1 << 16)) {
            // inject audio packet
            hdmi_inject_audio_packet(
                ctx,
                dl,
                dl->read_addr,
                dl->transfer_count,
                pkt_start,
#ifdef HDMI_AUDIO_FRAMESYNC_MODE_SUPPORT
                ctx->audio.samples_per_frame.count
#else 
                4
#endif
            );
#ifdef HDMI_AUDIO_FRAMESYNC_MODE_SUPPORT
            // decrement sample count
            if (ctx->modeflags & DVI_MODE_FLAGS_HDMI_AUDIO_FRAMESYNC) {
                ctx->audio.samples_per_frame.count -= 4;
            }
#endif
        }
    }
    if (ctx->state != DVI_STATE_ACTIVE_BLANK && ctx->state != DVI_STATE_SYNC_HDMI_PACKET_PRE && ctx->state != DVI_STATE_ACTIVE_NOP) {
        ctx->scanline++;
    }
    ctx->state = next_state;
}
#else
// TODO copy old code
#endif


void __not_in_flash_func(dvi_fill_cmdlist)() {
    struct dvi_dma_cmd_list *dl = &dma_cmdlist[dvi_sm.cmdlist_idx];
    for (int s = 0; s < COMMAND_LIST_LENGTH; s++) {
        dvi_state_advance(&dvi_sm, dl++, s);
    }
    dvi_sm.cmdlist_idx ^= COMMAND_LIST_LENGTH;
}

void dvi_init_cmdlist(uint32_t dma_ctrl, uint32_t dma_ctrl_chain) {
    struct dvi_dma_cmd_list *dl = &dma_cmdlist[0];
    for (int e = 0; e < COMMAND_LIST_COUNT; e++) {
        for (int s = 0; s < COMMAND_LIST_LENGTH; s++) {
            dl->write_addr = (uintptr_t)&hstx_fifo_hw->fifo;
            dl->ctrl_trig  = (s == COMMAND_LIST_LENGTH-1) ? dma_ctrl_chain : dma_ctrl;
            dl++;
        }
    }
}

// ---------------------------------

volatile int irq_count = 0;
// DVI user IRQ handler for the callback
void __not_in_flash_func(dvi_cb_irq_handler()) {
    if (dvi_sm.linebuf.task.state & DVI_CB_STATE_REQUEST) {
        gpio_put(2, 1);
        // process

        // null checks for lamers, let them get their deserved hard fault :grins:
        dvi_sm.linebuf.cb.proc(&dvi_sm.linebuf.task, dvi_sm.linebuf.cb.priv);

        // go to idle if done
        dvi_sm.linebuf.task.line += dvi_sm.linebuf.task.height;
        dvi_sm.linebuf.task.state = DVI_CB_STATE_IDLE;

        gpio_put(2, 0);
    }
    // acknowledge callback IRQ
    *dvi_sm.cb_irq_ack.icpr = dvi_sm.cb_irq_ack.mask;
}

#if HDMI_HSTX
// HDMI audio IRQ handler for the callback
void __not_in_flash_func(hdmi_audio_cb_irq_handler()) {
    struct dvi_sm_state_t *ctx = &dvi_sm;

    gpio_put(2, 1);
    // request as single chunk
    ctx->audio.cb.proc(
        ctx->audio.buf.ptr + ctx->audio.task.write_ptr*2,
        ctx->audio.task.frames_to_render,
        ctx->audio.cb.priv
    );
    ctx->audio.buf.avail_add = ctx->audio.task.frames_to_render;
    gpio_put(2, 0);

    // acknowledge callback IRQ
    *ctx->audio_cb_irq_ack.icpr = ctx->audio_cb_irq_ack.mask;
}
#endif

// DVI DMA IRQ handler
// interrupt handler (TODO: add more stuff to do =)
void __not_in_flash_func(dvi_dma_irq_handler()) {
    gpio_put(3, 1);
    dvi_fill_cmdlist();
    
    //dma_irqn_acknowledge_channel(dvi_sm.res.irq_dma - DMA_IRQ_0, dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]);
    *dvi_sm.dma_irq_ack.ints = dvi_sm.dma_irq_ack.mask;
    
    gpio_put(3, 0);
}

// ------------------------------
// free-running output init

// get memory required for the mode
void dvi_linebuf_get_memsize(struct dvi_timings_t *timings, uint32_t *linebuf_memsize, int pixel_rep) {
    if (timings == NULL || linebuf_memsize == NULL) return;

    *linebuf_memsize = ((timings->h.active / pixel_rep) * sizeof(uint32_t) * LINES_PER_BUFFER * LINE_BUFFER_COUNT);
}

// reset DVI line buffer stuff
int dvi_linebuf_reset() {
    // TODO
    return 0;
}

// set timings
int dvi_linebuf_set_timings(const struct dvi_timings_t *timings, int pix_rep) {
    if (timings == NULL) return 1;
    dvi_sm.timings = *timings;

    // apply pixel repetition
    dvi_sm.timings.pixelrep.active       = dvi_sm.timings.h.active       / pix_rep;
    dvi_sm.timings.pixelrep.border_left  = dvi_sm.timings.h.border_left  / pix_rep;
    dvi_sm.timings.pixelrep.border_right = dvi_sm.timings.h.border_right / pix_rep;

    // calcualte next states
    if (dvi_sm.timings.v.border_top > 0) {
        dvi_sm.next_state.back_porch_end = DVI_STATE_TOP_BORDER;
        dvi_sm.next_state.back_porch_end_scanlines = dvi_sm.timings.v.border_top;
    } else {
        dvi_sm.next_state.back_porch_end = DVI_STATE_ACTIVE_NOP;
        dvi_sm.next_state.back_porch_end_scanlines = dvi_sm.timings.v.active;
    }

    if (dvi_sm.timings.v.border_bottom > 0) {
        dvi_sm.next_state.active_pixels_end = DVI_STATE_BOTTOM_BORDER;
        dvi_sm.next_state.active_pixels_end_scanlines = dvi_sm.timings.v.border_bottom;
    } else {
        dvi_sm.next_state.active_pixels_end = DVI_STATE_FRONT_PORCH;
        dvi_sm.next_state.active_pixels_end_scanlines = dvi_sm.timings.v.front_porch;
    }

    return 0;
}

int vga_linebuf_set_timings(const struct dvi_timings_t *timings) {
    return dvi_linebuf_set_timings(timings, 1);     // pix_rep already accounted for
}

// set resources
int dvi_linebuf_set_resources(struct dvi_resources_t *resources, uint32_t *linebuf) {
    if (resources == NULL || linebuf == NULL) return 1;
    dvi_sm.res = *resources;
    for (int i = 0; i < LINE_BUFFER_COUNT; i++) {
        dvi_linebuf[i] = linebuf + (i * dvi_sm.timings.pixelrep.active * LINES_PER_BUFFER);
    }
    return 0;
}

// set line repetition factor (affects line buffer rendering as well!)
void dvi_linebuf_set_line_rep(int line_rep) {
    dvi_sm.linerep.reload = line_rep;
}

int hdmi_linebuf_init_info(int avi_b1, int avi_b2, int avi_b3, int vic, int sample_rate, int flags)
{
    data_packet_t packets[4];
    data_packet_t pkt_sample;
    int total_packets = 0;

    // add AVInfo packet
    set_AVI_info_frame(&packets[0], avi_b1, avi_b2, avi_b3, vic); total_packets++;
    if (sample_rate > 0) {
        uint32_t audio_n, audio_cts;

        // calculate sample rate from given frame rate
        // TODO: currently the only sample rates supported are 44100 and 48000 kHz
        switch (sample_rate) {
            case 44100:
                audio_n = 6272;
                break;
            default:
                if (sample_rate % 1000) return 1; // allow only khz-rounded values
                audio_n = 128 * (sample_rate / 1000);
                break;
        }
        audio_cts = ((uint64_t)dvi_sm.timings.pixelclock * audio_n) / (128ULL * sample_rate);

        // init info frames
        set_audio_info_frame(&packets[1], sample_rate);
        set_audio_clock_regeneration(&packets[2], audio_cts, audio_n);
        total_packets += 2;

        // init audio stuff
        dvi_sm.modeflags |= DVI_MODE_FLAGS_HDMI_AUDIO;
        
        // determine how many samples we want to process in one frame
        uint32_t samples_per_frame = (((int64_t)sample_rate * (int64_t)(1<<16) * (int64_t)dvi_sm.timings.h.total * (int64_t)dvi_sm.timings.v.total) / dvi_sm.timings.pixelclock);
        uint32_t packets_per_frame = samples_per_frame >> 2; // 4 samples per packet
        uint32_t audio_lines       = (dvi_sm.timings.v.total);
        dvi_sm.audio.add = (packets_per_frame + audio_lines - 1) / (audio_lines);
        dvi_sm.audio.acc = dvi_sm.audio.add;

        printf("htotal=%d vtotal=%d\n", dvi_sm.timings.h.total, dvi_sm.timings.v.total);
        printf("N = %d, CTS = %d\n", audio_n, audio_cts);
        printf("refresh rate=%d mHz\n", dvi_sm.timings.refresh);
        printf("sample rate=%d, spf=%d.%03d, ppf=%d.%03d, add=%04X\n",
            sample_rate,
            samples_per_frame>>16, ((samples_per_frame&0xFFFF)*1000)>>16,
            packets_per_frame>>16, ((packets_per_frame&0xFFFF)*1000)>>16,
            dvi_sm.audio.add
        );

        // cook preambles
        dvi_sm.preamble[0] = tmds_sync_word[
            (dvi_sm.timings.flags & DVI_TIMINGS_SYNC_POLARITY_MASK) ^ 
            dvi_sm.hdmi_pkt.sync_xormask | 
            SYNC_WORD_DATA_PREAMBLE_OFFSET
        ];
        dvi_sm.preamble[1] = tmds_sync_word[
            (dvi_sm.timings.flags & DVI_TIMINGS_SYNC_POLARITY_MASK) ^ 
            dvi_sm.hdmi_pkt.sync_xormask ^ DVI_TIMINGS_V_NEG | 
            SYNC_WORD_DATA_PREAMBLE_OFFSET
        ];

#if 1
        if ((samples_per_frame & 0xFFFF) == 0) {
            // use frame sync timing mode for integer samples per frame count
            dvi_sm.modeflags |= DVI_MODE_FLAGS_HDMI_AUDIO_FRAMESYNC;
            printf("use framesync timing mode\n");
        }
        dvi_sm.audio.samples_per_frame.count = 
        dvi_sm.audio.samples_per_frame.reload = samples_per_frame >> 16;
#endif
    }

    // cook packets
    {
        // split first sync line into two parts - first carries audio packets if any,
        // second (tail) carries HDMI info frames
        struct dvi_timings_t *t = &dvi_sm.timings;
        uint32_t blank_reserved_len = 64;       // should be enough

        uint32_t* ctrl_word_cur = dvi_hstx_ctrl_infoframe_pre;
        uint32_t syncflags =  t->flags & DVI_TIMINGS_SYNC_POLARITY_MASK;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.front_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
        dvi_sm.hdmi_pkt.infoframe_pre_pkt_start = ctrl_word_cur - dvi_hstx_ctrl_infoframe_pre;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (blank_reserved_len);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
        dvi_sm.hdmi_pkt.infoframe_pre_len = ctrl_word_cur - dvi_hstx_ctrl_infoframe_pre;

        ctrl_word_cur    = dvi_hstx_ctrl_infoframe;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (W_PREAMBLE);
        *ctrl_word_cur++ = tmds_sync_word[(syncflags ^ DVI_TIMINGS_H_NEG) | SYNC_WORD_DATA_PREAMBLE_OFFSET];
        int len = encode_data_islands_hstx(ctrl_word_cur + 1, packets, total_packets, syncflags ^ DVI_TIMINGS_H_NEG);
        *ctrl_word_cur   = HSTX_CMD_RAW | len;
        ctrl_word_cur   += (len + 1);
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch + t->h.active - len - W_PREAMBLE - blank_reserved_len);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
        dvi_sm.hdmi_pkt.infoframe_len = ctrl_word_cur - dvi_hstx_ctrl_infoframe;
    }

    return 0;
}

// fill HSTX command list
int dvi_linebuf_fill_hstx_cmdlist(int is_hdmi) {
    struct dvi_timings_t *t = &dvi_sm.timings;
    bool v_border = t->v.border_top  || t->v.border_bottom;
    bool h_border = t->pixelrep.border_left || t->pixelrep.border_right;

    // determine where to inject HDMI packets
    if (is_hdmi) {
        // EDIT: injects in vsync only for now
        dvi_sm.hdmi_pkt.sync_xormask = 0;
    }

    // cook control words
    {
        uint32_t* ctrl_word_cur;
        uint32_t syncflags =  t->flags & DVI_TIMINGS_SYNC_POLARITY_MASK;

        // nop =)
        dvi_hstx_ctrl_nop[0] = HSTX_CMD_NOP;

        // vsync
        ctrl_word_cur = dvi_hstx_ctrl_vsync_on;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_right + t->h.front_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
#if HDMI_HSTX
        dvi_sm.ctrl_pkt_start.vsync_on = ctrl_word_cur - dvi_hstx_ctrl_vsync_on;
#endif
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch + t->h.border_left + t->h.active);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
        *ctrl_word_cur++ = HSTX_CMD_NOP;
        dvi_sm.ctrl_len.vsync_on = ctrl_word_cur - dvi_hstx_ctrl_vsync_on;

        // vblank
        ctrl_word_cur = dvi_hstx_ctrl_vsync_off;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_right + t->h.front_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
#if HDMI_HSTX
        dvi_sm.ctrl_pkt_start.vsync_off = ctrl_word_cur - dvi_hstx_ctrl_vsync_off;
#endif
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch + t->h.border_left + t->h.active);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_NOP;
        dvi_sm.ctrl_len.vsync_off = ctrl_word_cur - dvi_hstx_ctrl_vsync_off;

        // vertical border
        ctrl_word_cur = dvi_hstx_ctrl_v_border;
        if (t->h.border_right) {
            *ctrl_word_cur++ = HSTX_CMD_TMDS_REPEAT | (t->h.border_right);
            *ctrl_word_cur++ = 0;
        }
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.front_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
#if HDMI_HSTX
        dvi_sm.ctrl_pkt_start.v_border = ctrl_word_cur - dvi_hstx_ctrl_v_border;
#endif
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
#if HDMI_HSTX
        if (is_hdmi) {
            // inject video leading guard band
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch - W_PREAMBLE - 2);
            *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (W_PREAMBLE);
            *ctrl_word_cur++ = tmds_sync_word[(syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG) | SYNC_WORD_PREAMBLE_OFFSET];
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (2);
            *ctrl_word_cur++ = TMDS_VIDEO_LEADING_GUARD_BAND;
        }
        else
#endif
        {
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch);
            *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        }
        *ctrl_word_cur++ = HSTX_CMD_TMDS_REPEAT | (t->h.border_left + t->h.active);
        *ctrl_word_cur++ = 0;
        dvi_sm.ctrl_len.v_border = ctrl_word_cur - dvi_hstx_ctrl_v_border;

        // active
        ctrl_word_cur = dvi_hstx_ctrl_v_active;
        if (t->h.border_right) {
            *ctrl_word_cur++ = HSTX_CMD_TMDS_REPEAT | (t->h.border_right);
            *ctrl_word_cur++ = 0;
        }
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.front_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
#if HDMI_HSTX
        dvi_sm.ctrl_pkt_start.v_active = ctrl_word_cur - dvi_hstx_ctrl_v_active;
#endif
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
#if HDMI_HSTX
        if (is_hdmi) {
            // inject video leading guard band
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch - W_PREAMBLE - 2);
            *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (W_PREAMBLE);
            *ctrl_word_cur++ = tmds_sync_word[(syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG) | SYNC_WORD_PREAMBLE_OFFSET];
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (2);
            *ctrl_word_cur++ = TMDS_VIDEO_LEADING_GUARD_BAND;
        }
        else
#endif
        {
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.back_porch);
            *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        }
        if (t->h.border_left) {
            *ctrl_word_cur++ = HSTX_CMD_TMDS_REPEAT | (t->h.border_left);
            *ctrl_word_cur++ = 0;
        }
        *ctrl_word_cur++ = HSTX_CMD_TMDS | (t->h.active);
        dvi_sm.ctrl_len.v_active = ctrl_word_cur - dvi_hstx_ctrl_v_active;
    }

#if HDMI_HSTX
    if (is_hdmi) {
        dvi_sm.modeflags |= DVI_MODE_FLAGS_HDMI_PACKETS;
    }
#endif
}

// fill HSTX command list
int vga_linebuf_fill_hstx_cmdlist() {
    struct dvi_timings_t *t = &dvi_sm.timings;
    bool v_border = t->v.border_top  || t->v.border_bottom;
    bool h_border = t->pixelrep.border_left || t->pixelrep.border_right;

    // cook control words
    {
        uint32_t* ctrl_word_cur;
        uint32_t syncflags =  t->flags & DVI_TIMINGS_SYNC_POLARITY_MASK;
        
        // nop =)
        dvi_hstx_ctrl_nop[0] = HSTX_CMD_NOP;

        // vsync
        ctrl_word_cur = dvi_hstx_ctrl_vsync_on;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_right + t->h.front_porch);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = vga_sync_word[syncflags];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch + t->h.border_left + t->h.active);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
        *ctrl_word_cur++ = HSTX_CMD_NOP;
        dvi_sm.ctrl_len.vsync_on = ctrl_word_cur - dvi_hstx_ctrl_vsync_on;

        // vblank
        ctrl_word_cur = dvi_hstx_ctrl_vsync_off;
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_right + t->h.front_porch);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch + t->h.border_left + t->h.active);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_NOP;
        dvi_sm.ctrl_len.vsync_off = ctrl_word_cur - dvi_hstx_ctrl_vsync_off;

        // vertical border
        ctrl_word_cur = dvi_hstx_ctrl_v_border;
        if (t->h.border_right) {
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_right);
            *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG]; // blank here, maintain sync pulses
        }
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.front_porch);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_left + t->h.active);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG]; // blank here, maintain sync pulses
        dvi_sm.ctrl_len.v_border = ctrl_word_cur - dvi_hstx_ctrl_v_border;

        // active
        ctrl_word_cur = dvi_hstx_ctrl_v_active;
        if (t->h.border_right) {
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_right);
            *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG]; // blank here, maintain sync pulses
        }
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.front_porch);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.sync);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.back_porch);
        *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        if (t->h.border_left) {
            *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.border_left);
            *ctrl_word_cur++ = vga_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG]; // blank here, maintain sync pulses
        }
        *ctrl_word_cur++ = HSTX_CMD_RAW | (t->h.active);
        dvi_sm.ctrl_len.v_active = ctrl_word_cur - dvi_hstx_ctrl_v_active;
    }
}

// initialize DMA channels
int dvi_linebuf_init_dma() {
    // configure DMA channels:
    // 1. display list channel - writes 4 words to SRAM->HSTX channel control block and triggers it
    // 2. SRAM->HSTX channel   - transfers data, chains to channel 1 or (on last line) channel 3 to restart display list
    // 3. DL reset channel     - resets channel 3 to start of list for the next frame

    // setup display list channel
    dma_channel_config dmacfg_dl;
    dmacfg_dl = dma_channel_get_default_config(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST]);
    channel_config_set_read_increment (&dmacfg_dl, true);
    channel_config_set_write_increment(&dmacfg_dl, true);
    channel_config_set_transfer_data_size(&dmacfg_dl, DMA_SIZE_32);
    channel_config_set_ring(&dmacfg_dl, true, 4);      // wrap writes on DMA control block
    dma_channel_configure(
        dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST],
        &dmacfg_dl,
        dma_channel_hw_addr(dvi_sm.res.dma_channels[DVI_DMACH_SRAM_HSTX]),
        &dma_cmdlist[0],
        4,
        false
    );

    // setup SRAM->HSTX transfer channel
    dma_channel_config dmacfg_img;
    dmacfg_img = dma_channel_get_default_config(dvi_sm.res.dma_channels[DVI_DMACH_SRAM_HSTX]);
    channel_config_set_chain_to(&dmacfg_img, dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST]);        // restart display list channel to reinit data channel
    channel_config_set_read_increment (&dmacfg_img, true);
    channel_config_set_write_increment(&dmacfg_img, false);
    channel_config_set_dreq(&dmacfg_img, DREQ_HSTX);
    channel_config_set_high_priority(&dmacfg_img, true);
    channel_config_set_transfer_data_size(&dmacfg_img, DMA_SIZE_32);
    // fill dvi state machine
    uint32_t dma_hstx_ctrl       = channel_config_get_ctrl_value(&dmacfg_img);
    channel_config_set_chain_to(&dmacfg_img, dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]);
    uint32_t dma_hstx_ctrl_chain = channel_config_get_ctrl_value(&dmacfg_img);

    // setup display list reset channel
    dma_channel_config dmacfg_dl_reset;
    dmacfg_dl_reset = dma_channel_get_default_config(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]);
    channel_config_set_read_increment (&dmacfg_dl_reset, true);
    channel_config_set_write_increment(&dmacfg_dl_reset, false);
    channel_config_set_ring(&dmacfg_dl_reset, false, 3);      // wrap writes on cmdlist reset ptr
    dma_channel_configure(
        dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET],
        &dmacfg_dl_reset,
        &dma_channel_hw_addr(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST])->al3_read_addr_trig,
        &dma_cmdlist_read_addr[0],
        1,
        false
    );

    // init command list
    dvi_init_cmdlist(dma_hstx_ctrl, dma_hstx_ctrl_chain);

    // install IRQ handlers
    dma_irqn_set_channel_enabled(dvi_sm.res.irq_dma - DMA_IRQ_0, dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET], true);
    irq_set_exclusive_handler(dvi_sm.res.irq_dma, dvi_dma_irq_handler);
    irq_set_exclusive_handler(dvi_sm.res.irq_linebuf_callback, dvi_cb_irq_handler);
    irq_set_priority(dvi_sm.res.irq_dma, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_priority(dvi_sm.res.irq_linebuf_callback, PICO_DEFAULT_IRQ_PRIORITY - 0x10); // must be lower priority than hardware IRQs but higher than other user callbacks
    irq_set_enabled(dvi_sm.res.irq_dma, true);
    irq_set_enabled(dvi_sm.res.irq_linebuf_callback, true);

#if HDMI_HSTX
    if (dvi_sm.modeflags & DVI_MODE_FLAGS_HDMI_AUDIO) {
        irq_set_exclusive_handler(dvi_sm.res.irq_audio_callback, hdmi_audio_cb_irq_handler);
        irq_set_priority(dvi_sm.res.irq_audio_callback, PICO_DEFAULT_IRQ_PRIORITY + 0x10); // must be lower priority than hardware IRQs but higher than other user callbacks
        irq_set_enabled(dvi_sm.res.irq_audio_callback, true);
    }
#endif

    // precalculate stuff for faster irq_clear()
    dvi_sm.cb_irq_ack.ispr = &nvic_hw->ispr[dvi_sm.res.irq_linebuf_callback/32];
    dvi_sm.cb_irq_ack.icpr = &nvic_hw->icpr[dvi_sm.res.irq_linebuf_callback/32];
    dvi_sm.cb_irq_ack.mask = (1 << (dvi_sm.res.irq_linebuf_callback & 31));

#if HDMI_HSTX
    dvi_sm.audio_cb_irq_ack.ispr = &nvic_hw->ispr[dvi_sm.res.irq_audio_callback/32];
    dvi_sm.audio_cb_irq_ack.icpr = &nvic_hw->icpr[dvi_sm.res.irq_audio_callback/32];
    dvi_sm.audio_cb_irq_ack.mask = (1 << (dvi_sm.res.irq_audio_callback & 31));
#endif

    dvi_sm.dma_irq_ack.ints = &dma_hw->irq_ctrl[dvi_sm.res.irq_dma - DMA_IRQ_0].ints;
    dvi_sm.dma_irq_ack.inte = &dma_hw->irq_ctrl[dvi_sm.res.irq_dma - DMA_IRQ_0].inte;
    dvi_sm.dma_irq_ack.mask = (1 << (dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]));
}

// set line buffer render callback
void dvi_linebuf_set_cb(dvi_linebuf_cb_t cb, void *priv) {
    // disable DMA interrupts for the update time, and also synchronize updates with VSYNC
    while (dvi_sm.state & DVI_STATE_SYNC_BIT) tight_loop_contents();
    irq_set_enabled(dvi_sm.res.irq_dma, false);
    dvi_sm.linebuf.cb_latch.proc = cb;
    dvi_sm.linebuf.cb_latch.priv = priv;
    irq_set_enabled(dvi_sm.res.irq_dma, true);
}

#if HDMI_HSTX
// set audio buffer properties
void hdmi_audio_set_buffer(int16_t *buf, int samples_in_buffer) {
    if ((buf == NULL) || (samples_in_buffer <= 0) || ((samples_in_buffer & 3) != 0)) return;

    // likewise, synchronize updates
    while (dvi_sm.state & DVI_STATE_SYNC_BIT) tight_loop_contents();
    irq_set_enabled(dvi_sm.res.irq_dma, false);
    dvi_sm.audio.buf.ptr        = buf;
    dvi_sm.audio.buf.size       = samples_in_buffer;
    dvi_sm.audio.buf.size_half  = samples_in_buffer / 2;
    dvi_sm.audio.buf.read_ptr   = 0;
    dvi_sm.audio.buf.avail      = 0;
    dvi_sm.audio.buf.avail_add  = 0;
    dvi_sm.audio.buf.framect    = 0;

    dvi_sm.audio.task.write_ptr = 0;
    dvi_sm.audio.task.frames_to_render = 0;
    irq_set_enabled(dvi_sm.res.irq_dma, true);
}

// set audio buffer callback
void hdmi_audio_set_cb(hdmi_audio_cb_t cb, void *priv) {
    // disable DMA interrupts for the update time, and also synchronize updates with VSYNC
    while (dvi_sm.state & DVI_STATE_SYNC_BIT) tight_loop_contents();
    irq_set_enabled(dvi_sm.res.irq_dma, false);
    dvi_sm.audio.cb_latch.proc = cb;
    dvi_sm.audio.cb_latch.priv = priv;
    irq_set_enabled(dvi_sm.res.irq_dma, true);
}
#endif

// start DVI line buffer output
int dvi_linebuf_start() {
    dvi_sm.audio.buf.read_ptr = 0;
    dvi_sm.audio.buf.idx = 0;
    dvi_sm.audio.buf.framect = 0;
    dvi_sm.audio.buf.avail = 0;
    dvi_sm.audio.buf.avail_add = 0;

    dvi_sm.scanline = 0;
    dvi_sm.cmdlist_idx = 0; // to fill it entirely + advance state 
    dvi_sm.state = (dvi_sm.modeflags & DVI_MODE_FLAGS_HDMI_PACKETS) ? DVI_STATE_SYNC_HDMI_PACKET_PRE : DVI_STATE_SYNC;
    dvi_sm.scanlines_state = dvi_sm.timings.v.sync;
    dvi_sm.linebuf.task.state = DVI_CB_STATE_IDLE;

    // this one advances the state machine each IRQ
    // CAUTION - may call callback!
    dvi_fill_cmdlist();
    dvi_fill_cmdlist();

    if (dvi_sm.modeflags & DVI_MODE_FLAGS_HDMI_AUDIO) {
        dvi_sm.audio.cb.proc(
            dvi_sm.audio.buf.ptr,
            dvi_sm.audio.buf.size,
            dvi_sm.audio.cb.priv
        );
        dvi_sm.audio.buf.avail_add = dvi_sm.audio.buf.size;
    }

    // set high DMA priority
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    // start DMA transfer
    dma_channel_start(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST]);

    return 0;
}

// stop DVI line buffer output
int dvi_linebuf_stop() {
    hw_clear_bits(&dma_channel_hw_addr(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST])->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_clear_bits(&dma_channel_hw_addr(dvi_sm.res.dma_channels[DVI_DMACH_SRAM_HSTX])->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_clear_bits(&dma_channel_hw_addr(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET])->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_abort(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST]);
    dma_channel_abort(dvi_sm.res.dma_channels[DVI_DMACH_SRAM_HSTX]);
    dma_channel_abort(dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]);

    // reset state machine
    dvi_sm.state    = DVI_STATE_IDLE;

    return 0;
}

// deinitialize stuff
int dvi_linebuf_done() {
    dma_irqn_set_channel_enabled(dvi_sm.res.irq_dma - DMA_IRQ_0, dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET], false);
    irq_set_enabled(dvi_sm.res.irq_dma, false);
    irq_set_enabled(dvi_sm.res.irq_linebuf_callback, false);
    irq_remove_handler(dvi_sm.res.irq_dma, dvi_dma_irq_handler);
    irq_remove_handler(dvi_sm.res.irq_linebuf_callback, dvi_cb_irq_handler);
    irq_set_priority(dvi_sm.res.irq_dma, PICO_DEFAULT_IRQ_PRIORITY);
    irq_set_priority(dvi_sm.res.irq_linebuf_callback, PICO_DEFAULT_IRQ_PRIORITY);
    if (dvi_sm.modeflags & DVI_MODE_FLAGS_HDMI_AUDIO) {
        irq_set_enabled(dvi_sm.res.irq_audio_callback, false);
        irq_remove_handler(dvi_sm.res.irq_audio_callback, hdmi_audio_cb_irq_handler);
        irq_set_priority(dvi_sm.res.irq_audio_callback, PICO_DEFAULT_IRQ_PRIORITY);
    }

    // clear state machine data
    memset(&dvi_sm, 0, sizeof(dvi_sm));

    return 0;
}

volatile int dvi_linebuf_get_state() {
    return dvi_sm.state;
}

volatile int dvi_linebuf_get_frame_count() {
    return dvi_sm.frame;
}

struct dvi_sm_state_t* dvi_linebuf_get_sm() {
    return &dvi_sm;
}

#if VGA_HSTX
// 4bpp to PWM index translation table
#define VGA_HSTX_PHASE(p3, p2, p1, p0) (p3 << 24) | (p2 << 16) | (p1 << 8) | (p0 << 0)

static uint32_t __not_in_flash("vga_hstx_pwm.data") vga_pwm_xlat_table[16] = {
    VGA_HSTX_PHASE(0, 0, 0, 0),         //  0 ->  0
    VGA_HSTX_PHASE(0, 0, 0, 1),         //  1 ->  1
    VGA_HSTX_PHASE(0, 1, 0, 1),         //  2 ->  2
    VGA_HSTX_PHASE(0, 1, 0, 1),         //  3 ->  2
    VGA_HSTX_PHASE(0, 1, 1, 1),         //  4 ->  3
    VGA_HSTX_PHASE(1, 1, 1, 1),         //  5 ->  4
    VGA_HSTX_PHASE(1, 1, 1, 2),         //  6 ->  5
    VGA_HSTX_PHASE(1, 2, 1, 2),         //  7 ->  6
    VGA_HSTX_PHASE(1, 2, 1, 2),         //  8 ->  6
    VGA_HSTX_PHASE(1, 2, 2, 2),         //  9 ->  7
    VGA_HSTX_PHASE(2, 2, 2, 2),         // 10 ->  8
    VGA_HSTX_PHASE(2, 2, 2, 3),         // 11 ->  9
    VGA_HSTX_PHASE(2, 3, 2, 3),         // 12 -> 10
    VGA_HSTX_PHASE(2, 3, 2, 3),         // 13 -> 10
    VGA_HSTX_PHASE(2, 3, 3, 3),         // 14 -> 11
    VGA_HSTX_PHASE(3, 3, 3, 3),         // 15 -> 12
};

uint32_t __not_in_flash("vga_hstx_pwm.text") vga_pwm_get_pixel_sync_mask() {
    return vga_sync_word[(dvi_sm.timings.flags & DVI_TIMINGS_SYNC_POLARITY_MASK) ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
}

uint32_t __not_in_flash("vga_hstx_pwm.text") vga_pwm_xlat_color(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t rtn = vga_pwm_get_pixel_sync_mask();
    rtn |= (vga_pwm_xlat_table[b >> 4] << 0);
    rtn |= (vga_pwm_xlat_table[g >> 4] << 2);
    rtn |= (vga_pwm_xlat_table[r >> 4] << 4);
    return rtn;
}

uint32_t __not_in_flash("vga_hstx_pwm.text") vga_pwm_xlat_color32(uint32_t idx) {
    return vga_pwm_xlat_color((idx >> 16) & 0xFF, (idx >> 8) & 0xFF, (idx >> 0) & 0xFF);
}
void __not_in_flash("vga_hstx_pwm.text") vga_pwm_xlat_palette(uint32_t *dst, uint8_t *src, uint32_t colors, uint32_t src_pitch) {
    uint32_t syncmask = vga_pwm_get_pixel_sync_mask();
    if (colors != 0) do {
        uint32_t rtn = syncmask;
        rtn |= (vga_pwm_xlat_table[src[0] >> 4] << 0);
        rtn |= (vga_pwm_xlat_table[src[1] >> 4] << 2);
        rtn |= (vga_pwm_xlat_table[src[2] >> 4] << 4);
        *dst++ = rtn; src += src_pitch;
    } while (--colors);
}
#endif
