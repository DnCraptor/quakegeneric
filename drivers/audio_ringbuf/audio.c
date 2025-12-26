// simple DMA-driven I2S/PWM audio driver
// --wbcbz7 27.12.2o25

// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>
#include <stdlib.h>
#include <pico/stdlib.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include "audio.h"
#include "audio_i2s.pio.h"

// -------------------------------------
// audio driver stuff

// ring buffer restart positions for RP2040-style audio DMA
static __aligned(sizeof(int16_t*)*2) __not_in_flash() int16_t* audiobuf_ring_restart[2];

enum {
    AUDIO_DMA_CHAN_MAIN = 0,
    AUDIO_DMA_CHAN_RESTART
};

static audio_config_t audiocfg;

// audio interrupt handler
static void __not_in_flash_func(audio_dma_interrupt_handler)() {
    if (dma_irqn_get_channel_status(audiocfg.dma_irq - DMA_IRQ_0, audiocfg.dma_channels[AUDIO_DMA_CHAN_RESTART])) {
        audiocfg.samples_played += audiocfg.buffer_half_size;

        // determine which buffer to update, based on current DMA read address
        // (trying to recover from the situation when we missed the interrupt by some reason and
        // filling the same buffer the DMA is reading from)
        uint32_t read_addr = dma_channel_hw_addr(audiocfg.dma_channels[AUDIO_DMA_CHAN_MAIN])->read_addr;
        if (read_addr >= (audiocfg.dma_buffer + audiocfg.buffer_half_smps)) {
            audiocfg.buffer_offset = 0;
        } else {
            audiocfg.buffer_offset = audiocfg.buffer_half_smps;
        }
        
        // start callback
        audiocfg.cb(audiocfg.dma_buffer + audiocfg.buffer_offset, audiocfg.buffer_half_size, audiocfg.cb_priv, &audiocfg);
        audiocfg.samples_rendered += audiocfg.buffer_half_size;
        dma_irqn_acknowledge_channel(audiocfg.dma_irq - DMA_IRQ_0, audiocfg.dma_channels[AUDIO_DMA_CHAN_RESTART]);
    }
}

// get default audio config
audio_config_t* audio_init_default_cfg() {
    memset(&audiocfg, 0, sizeof(audiocfg));
    audiocfg.volume           = 32767;        // maximum
    audiocfg.sample_freq      = 48000;
    audiocfg.dma_irq_priority = PICO_DEFAULT_IRQ_PRIORITY;
    audiocfg.i2s.sm           = -1;             // find first suitable SM
    audiocfg.i2s.prog_ofs     = -1;             // locate first suitable prog offset
    return &audiocfg;
}

void audio_start() {
    dma_channel_start(audiocfg.dma_channels[AUDIO_DMA_CHAN_MAIN]);        // start audio DMA channel
}

void audio_pause() {
    dma_channel_abort(audiocfg.dma_channels[AUDIO_DMA_CHAN_MAIN]);
}

void audio_stop() {
    // TODO:: pause and clear state
}

// set volume (0 - min, 32767 - max)
void audio_set_volume(int16_t volume) {
    if (volume < 0) volume = 0;

    audiocfg.volume = volume;
    if (audiocfg.flags & AUDIO_CFG_PWM) {
        audiocfg.volscale = (audiocfg.volume * audiocfg.pwm.max_level) >> 15; 
    } else {
        audiocfg.volscale = volume;
    }
}

int audio_get_volume() {
    return audiocfg.volume;
}

void audio_done() {
    audio_config_t *ctx = &audiocfg;

    dma_channel_abort(ctx->dma_channels[AUDIO_DMA_CHAN_RESTART]);
    dma_channel_abort(ctx->dma_channels[AUDIO_DMA_CHAN_MAIN]);

    dma_irqn_set_channel_enabled(ctx->dma_irq - DMA_IRQ_0, ctx->dma_channels[AUDIO_DMA_CHAN_RESTART], false);
    if (ctx->flags & AUDIO_CFG_IRQ_EXCLUSIVE) {
        irq_set_priority(ctx->dma_irq, PICO_DEFAULT_IRQ_PRIORITY);
        irq_set_enabled(ctx->dma_irq, false);
    }
    irq_remove_handler(ctx->dma_irq, audio_dma_interrupt_handler);

    if (ctx->flags & AUDIO_CFG_PWM) {
        pwm_set_enabled(ctx->pwm.slice, false);
    } else {
        pio_sm_set_enabled(ctx->i2s.pio, ctx->i2s.sm, false);
    }
}

// returns position in samples
uint32_t audio_get_position() {
    uint32_t  old_pos            = audiocfg.samples_played_last, new_pos;
    io_rw_32* dma_transfer_count = &dma_channel_hw_addr(audiocfg.dma_channels[AUDIO_DMA_CHAN_MAIN])->transfer_count;
    io_rw_32* samples_played     = (io_rw_32*)&audiocfg.samples_played; 
    uint32_t  tc, tbuf1, tbuf2;
    int timeout = 5;
    do {
        tbuf1 = *samples_played;
        tc    = *dma_transfer_count;
        tbuf2 = *samples_played;
        if ((tbuf1 == tbuf2) && ((new_pos = (tbuf1 + (audiocfg.buffer_half_size - tc))) >= old_pos)) break;
        sleep_us(1);
    } while (--timeout);
    if (timeout > 0) audiocfg.samples_played_last = new_pos; else new_pos = old_pos;
    return new_pos;
}

// apply audio config
int audio_init() {
    audio_config_t *ctx = &audiocfg;

    // validate 
    if (ctx->cb == NULL || ctx->dma_buffer == NULL)   return AUDIO_ERR_NULLPTR; 
    if (ctx->dma_channels[0] == ctx->dma_channels[1]) return AUDIO_ERR_INVALID_PARAMS;
    if (ctx->buffer_size == 0) return AUDIO_ERR_INVALID_PARAMS;

    // prefill audio buffer
    ctx->buffer_half_size = ctx->buffer_size / 2;
    ctx->buffer_half_smps = ctx->buffer_half_size << (ctx->flags & AUDIO_CFG_MONO ? 0 : 1);
    audiobuf_ring_restart[0]  = ctx->dma_buffer + ctx->buffer_half_smps;
    audiobuf_ring_restart[1]  = ctx->dma_buffer;
    ctx->samples_played      = 0;
    ctx->samples_played_last = 0;
    ctx->samples_rendered    = 0;

    // set DMA
    dma_channel_config audio_cfg = dma_channel_get_default_config(ctx->dma_channels[AUDIO_DMA_CHAN_MAIN]);
    channel_config_set_transfer_data_size(&audio_cfg, (ctx->flags & AUDIO_CFG_MONO) ? DMA_SIZE_16 : DMA_SIZE_32); // hack with halfword replication to simulate mono without additional effort
    channel_config_set_chain_to(&audio_cfg, ctx->dma_channels[AUDIO_DMA_CHAN_RESTART]);
    channel_config_set_read_increment(&audio_cfg, true);
    channel_config_set_write_increment(&audio_cfg, false);
    
    dma_channel_config restart_cfg = dma_channel_get_default_config(ctx->dma_channels[AUDIO_DMA_CHAN_RESTART]);
    channel_config_set_transfer_data_size(&restart_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&restart_cfg, true);
    channel_config_set_write_increment(&restart_cfg, false);
    channel_config_set_ring(&restart_cfg, false, 3);      // log2(sizeof(int16_t*) * 2)

    // init audio
    if (ctx->flags & AUDIO_CFG_PWM) {
        // init PWM
        // get slice from base pin
        ctx->pwm.slice = pwm_gpio_to_slice_num(ctx->pwm.base_pin);
        gpio_set_function(ctx->pwm.base_pin^0, GPIO_FUNC_PWM);
        gpio_set_function(ctx->pwm.base_pin^1, GPIO_FUNC_PWM);

        pwm_config cfg = pwm_get_default_config();

        int max_wrap = clock_get_hz(clk_sys)/ctx->sample_freq;
        int clkdiv   = 1;
        while (max_wrap >= 65535) {
            clkdiv <<= 1; max_wrap >>= 1;
        };

        pwm_config_set_clkdiv_int_frac(&cfg, 1, 0);
        pwm_config_set_wrap(&cfg, max_wrap-1);
        pwm_init(ctx->pwm.slice, &cfg, true);

        // set dreq for DMA channel
        channel_config_set_dreq(&audio_cfg, pwm_get_dreq(ctx->pwm.slice));

        ctx->pwm.max_level = max_wrap >> 1;
    } else {
        // init I2S
        // add program
        int sm, prog_ofs;
        uint32_t div = (clock_get_hz(clk_sys) * 4) / ctx->sample_freq;
#ifdef I2S_CS4334
        const pio_program_t *prog = &audio_i2s_cs4334_program;
        div >>= 3;
#else
        const pio_program_t *prog = &audio_i2s_program;
#endif
        if (ctx->i2s.pio == NULL) {
            if (pio_claim_free_sm_and_add_program(prog, &ctx->i2s.pio, &sm, &prog_ofs) == false) return AUDIO_ERR_OUT_OF_PIO_SM;
            ctx->i2s.sm = sm; ctx->i2s.prog_ofs = prog_ofs;
        } else {
            if (ctx->i2s.sm == -1) ctx->i2s.sm = pio_claim_unused_sm(ctx->i2s.pio, false); if (ctx->i2s.sm == -1) return AUDIO_ERR_OUT_OF_PIO_SM;
            if ((ctx->i2s.prog_ofs = pio_add_program(ctx->i2s.pio, prog)) < 0) return AUDIO_ERR_OUT_OF_PIO_SM;
        }
        
        int pin_ofs = ctx->flags & AUDIO_CFG_I2S_GPIO_OFFSET ? 16 : 0;
        audio_i2s_program_init(ctx->i2s.pio, ctx->i2s.sm, ctx->i2s.prog_ofs, ctx->i2s.data_pin - pin_ofs, ctx->i2s.bclk_pin - pin_ofs);
        pio_sm_set_clkdiv_int_frac(ctx->i2s.pio, ctx->i2s.sm, div >> 8u, div & 0xFFu);
        pio_sm_set_enabled(ctx->i2s.pio, ctx->i2s.sm, true);

        // set pin function
        gpio_set_function(ctx->i2s.data_pin,     (gpio_function_t)((int)GPIO_FUNC_PIO0 + PIO_NUM(ctx->i2s.pio)));
        gpio_set_function(ctx->i2s.bclk_pin,     (gpio_function_t)((int)GPIO_FUNC_PIO0 + PIO_NUM(ctx->i2s.pio)));
        gpio_set_function(ctx->i2s.bclk_pin + 1, (gpio_function_t)((int)GPIO_FUNC_PIO0 + PIO_NUM(ctx->i2s.pio)));

        // set dreq for DMA channel
        channel_config_set_dreq(&audio_cfg, PIO_DREQ_NUM(ctx->i2s.pio, ctx->i2s.sm, true));
    }

    // apply volume
    audio_set_volume(ctx->volume);

    // advance callback
    for (int i = 0; i < 2; i++) {
        ctx->cb(ctx->dma_buffer + i*ctx->buffer_half_smps, ctx->buffer_half_size, ctx->cb_priv, ctx);
        ctx->samples_rendered += ctx->buffer_half_size;
    }

    dma_irqn_set_channel_enabled(ctx->dma_irq - DMA_IRQ_0, ctx->dma_channels[AUDIO_DMA_CHAN_RESTART], true);
    if (audiocfg.flags & AUDIO_CFG_IRQ_EXCLUSIVE) {
        irq_set_exclusive_handler(ctx->dma_irq, audio_dma_interrupt_handler);
        irq_set_priority(ctx->dma_irq, ctx->dma_irq_priority);
    } else {
        irq_add_shared_handler(ctx->dma_irq, audio_dma_interrupt_handler, ctx->dma_irq_priority);
    }
    irq_set_enabled(ctx->dma_irq, true);

    // setup DMA transfer for restart channel
    dma_channel_configure(
        ctx->dma_channels[AUDIO_DMA_CHAN_RESTART],
        &restart_cfg,
        &dma_channel_hw_addr(ctx->dma_channels[AUDIO_DMA_CHAN_MAIN])->al3_read_addr_trig,
        audiobuf_ring_restart,
        1,
        false
    );

    // setup DMA transfer for playback channel
    dma_channel_configure(
        ctx->dma_channels[AUDIO_DMA_CHAN_MAIN],
        &audio_cfg,
        ctx->flags & AUDIO_CFG_PWM ? &pwm_hw->slice[ctx->pwm.slice].cc : &ctx->i2s.pio->txf[ctx->i2s.sm],
        ctx->dma_buffer,
        ctx->buffer_half_size,
        false
    );

    return AUDIO_ERR_OK;
}
