// DVI/HSTX display driver for the RP2350 - work in progress
// --wbcbz7 11.o9.2o25

#include <stdio.h>

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
// control words
static uint32_t dvi_hstx_ctrl_nop[1];
static uint32_t dvi_hstx_ctrl_vsync_off[7];
static uint32_t dvi_hstx_ctrl_vsync_on[7];
static uint32_t dvi_hstx_ctrl_v_border[10];
static uint32_t dvi_hstx_ctrl_v_active[11]; // incl. blanks if present

// TODO: control words for HDMI audio support ;)

// --------------------------------
// command list
#define COMMAND_LIST_LENGTH 2
#define COMMAND_LIST_COUNT  2

// command list itself and read address pointers
static struct dvi_dma_cmd_list dma_cmdlist[COMMAND_LIST_LENGTH*COMMAND_LIST_COUNT];
static __not_in_flash() __aligned(8) uintptr_t dma_cmdlist_read_addr[] = {
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
static const uint32_t tmds_sync_word[4] = {
    TMDS_SYNC_V1_H1, TMDS_SYNC_V1_H0, TMDS_SYNC_V0_H1, TMDS_SYNC_V0_H0
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

enum {
    DVI_MODE_FLAGS_PIXEL_REP = (1 << 0),
    DVI_MODE_FLAGS_NO_H_BORDER = 0,
};

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
        timings->refresh = (timings->pixelclock * 1000LL / (timings->h.total * timings->v.total));
    } else {
        // TODO: recalculate pixel clock from desired refresh rate
        return 1;
    }

    return 0;
}

// ------------------------------
// very hacky inline version of irq_set_pending()
// (the pico-sdk one generates a call to flash. in an ISR. awesome :)
static inline void dvi_irq_set_pending(uint num) {
    *dvi_sm.cb_irq_ack.ispr = dvi_sm.cb_irq_ack.mask;
}

// fill dma command list entry, advance DVI state
static void __not_in_flash_func(dvi_state_advance)(struct dvi_sm_state_t *ctx, struct dvi_dma_cmd_list *dl, int dlidx) {
    if (ctx->state != DVI_STATE_ACTIVE_BLANK) ctx->scanline++;
    switch (ctx->state) {
        case DVI_STATE_IDLE:
            break;
        case DVI_STATE_SYNC:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_vsync_on;
            dl->transfer_count = ctx->ctrl_len.vsync_on;
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
                ctx->state = DVI_STATE_BACK_PORCH;
            }
            break;
        case DVI_STATE_BACK_PORCH:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_vsync_off;
            dl->transfer_count = ctx->ctrl_len.vsync_off;
            if (--ctx->scanlines_state == 0) {
                // advance
                ctx->state = ctx->next_state.back_porch_end;
                ctx->scanlines_state = ctx->next_state.back_porch_end_scanlines;
            }
            break;
        case DVI_STATE_TOP_BORDER:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_v_border;
            dl->transfer_count = ctx->ctrl_len.v_border;
            if (--ctx->scanlines_state == 0) {
                // advance
                ctx->state = DVI_STATE_ACTIVE_NOP;
                ctx->scanlines_state = ctx->timings.v.active;
            }
            break;
        case DVI_STATE_ACTIVE_NOP:
            if (dlidx == 0 || dlidx == COMMAND_LIST_LENGTH-1) ctx->state = DVI_STATE_ACTIVE_BLANK;
            if (dlidx != 0) {
                dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_nop;
                dl->transfer_count = count_of(dvi_hstx_ctrl_nop);
                break;
            } // fallthrough if dlidx == 0
        case DVI_STATE_ACTIVE_BLANK:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_v_active;
            dl->transfer_count = ctx->ctrl_len.v_active;
            ctx->state = DVI_STATE_ACTIVE_PIXELS;
            break;
        case DVI_STATE_ACTIVE_PIXELS:
            dl->read_addr      = (uintptr_t)(dvi_linebuf[ctx->linebuf.disp.idx] + ctx->linebuf.disp.pos);
            dl->transfer_count = ctx->timings.pixelrep.active;
            
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
                ctx->state = ctx->next_state.active_pixels_end;
                ctx->scanlines_state = ctx->next_state.active_pixels_end_scanlines;
            } else {
                ctx->state = DVI_STATE_ACTIVE_BLANK;
            }
            break;
        case DVI_STATE_BOTTOM_BORDER:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_v_border;
            dl->transfer_count = ctx->ctrl_len.v_border;
            if (--ctx->scanlines_state == 0) {
                // advance
                ctx->state = DVI_STATE_FRONT_PORCH;
                ctx->scanlines_state = ctx->timings.v.front_porch;
            }
            break;
        case DVI_STATE_FRONT_PORCH:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_vsync_off;
            dl->transfer_count = ctx->ctrl_len.vsync_off;
            if (--ctx->scanlines_state == 0) {
                // next frame
                // TODO: add alignment NOP for odd linecount
                ctx->frame++;
                ctx->scanline = 0;
                ctx->state = DVI_STATE_SYNC;
                ctx->scanlines_state = ctx->timings.v.sync;
            }
            break;
        case DVI_STATE_FRONT_PORCH_NOP:
            dl->read_addr      = (uintptr_t)dvi_hstx_ctrl_nop;
            dl->transfer_count = count_of(dvi_hstx_ctrl_nop);
            break;
    }
}

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
    //irq_clear(dvi_sm.res.irq_linebuf_callback);
    *dvi_sm.cb_irq_ack.icpr = dvi_sm.cb_irq_ack.mask;
}

// DVI DMA IRQ handler
// interrupt handler (TODO: add more stuff to do =)
void __not_in_flash_func(dvi_dma_irq_handler()) {
    gpio_put(3, 1);
    dvi_fill_cmdlist();
    
    //dma_irqn_acknowledge_channel(dvi_sm.res.irq_dma - DMA_IRQ_0, dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]);
    *dvi_sm.dma_irq_ack.irq_ctrl = dvi_sm.dma_irq_ack.mask;
    
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

// fill HSTX command list
int dvi_linebuf_fill_hstx_cmdlist() {
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
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG];
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
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT | (t->h.back_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
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
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.sync);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_V_NEG];
        *ctrl_word_cur++ = HSTX_CMD_RAW_REPEAT  | (t->h.back_porch);
        *ctrl_word_cur++ = tmds_sync_word[syncflags ^ DVI_TIMINGS_H_NEG ^ DVI_TIMINGS_V_NEG];
        if (t->h.border_left) {
            *ctrl_word_cur++ = HSTX_CMD_TMDS_REPEAT | (t->h.border_left);
            *ctrl_word_cur++ = 0;
        }
        *ctrl_word_cur++ = HSTX_CMD_TMDS | (t->h.active);
        dvi_sm.ctrl_len.v_active = ctrl_word_cur - dvi_hstx_ctrl_v_active;
    }
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

    // precalculate stuff for faster irq_clear()
    dvi_sm.cb_irq_ack.ispr = &nvic_hw->ispr[dvi_sm.res.irq_linebuf_callback/32];
    dvi_sm.cb_irq_ack.icpr = &nvic_hw->icpr[dvi_sm.res.irq_linebuf_callback/32];
    dvi_sm.cb_irq_ack.mask = (1 << (dvi_sm.res.irq_linebuf_callback & 31));

    dvi_sm.dma_irq_ack.irq_ctrl = &dma_hw->irq_ctrl[dvi_sm.res.irq_dma - DMA_IRQ_0].ints;
    dvi_sm.dma_irq_ack.mask = (1 << (dvi_sm.res.dma_channels[DVI_DMACH_DISPLAYLIST_RESET]));
}

// set line buffer render callback
void dvi_linebuf_set_cb(dvi_linebuf_cb_t cb, void *priv) {
    // disable DMA interrupts for the update time, and also synchronize updates with VSYNC
    while (dvi_sm.state == DVI_STATE_SYNC) tight_loop_contents();
    irq_set_enabled(dvi_sm.res.irq_dma, false);
    dvi_sm.linebuf.cb_latch.proc = cb;
    dvi_sm.linebuf.cb_latch.priv = priv;
    irq_set_enabled(dvi_sm.res.irq_dma, true);
}

// start DVI line buffer output
int dvi_linebuf_start() {
    dvi_sm.scanline = 0;
    dvi_sm.cmdlist_idx = 0; // to fill it entirely + advance state 
    dvi_sm.state = DVI_STATE_SYNC;
    dvi_sm.scanlines_state = dvi_sm.timings.v.sync;
    dvi_sm.linebuf.task.state = DVI_CB_STATE_IDLE;

    // this one advances the state machine each IRQ
    // CAUTION - may call callback!
    dvi_fill_cmdlist();
    dvi_fill_cmdlist();

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

    // clear fields
    dvi_sm.frame = 0;

    return 0;
}

volatile int dvi_linebuf_get_state() {
    return dvi_sm.state;
}

int dvi_linebuf_get_frame_count() {
    return dvi_sm.frame;
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
