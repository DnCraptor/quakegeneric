#include <stdint.h>
#include <stdbool.h>

#include <pico.h>
#include <hardware/pwm.h>
#include <pico/multicore.h>

#include "audio.h"
#include "mixer.h"

#include "quakedef.h"

#define AUDIO_BUFFER_SIZE_LOG2 8
#define AUDIO_BUFFER_SIZE      (1 << AUDIO_BUFFER_SIZE_LOG2)

static int16_t audiobuf[AUDIO_BUFFER_SIZE * 2]; // for stereo
bool is_i2s_enabled = false;

extern "C" qboolean CDAudio_GetSamples(int16_t* buf, size_t n); // FIXME!!

int __not_in_flash_func(audio_cb)(int16_t* dst, uint32_t frames, void* priv, const audio_config_t *cfg) {
    int volscale = cfg->volscale;
    int volbias  = cfg->flags & AUDIO_CFG_PWM ? 32768 : 0;
    if (frames == 0) return 0;
    
    // get CD samples (todo: do this another way)
    if (!CDAudio_GetSamples(dst, frames)) memset(dst, 0, sizeof(int16_t) * 2 * frames);

    do {
        dst[0] = (volscale * (dst[0] + volbias)) >> 15;
        dst[1] = (volscale * (dst[1] + volbias)) >> 15;
        dst += 2;
    } while (--frames);

    return 0;
}

void mixer_init() {
    audio_config_t *cfg = audio_init_default_cfg();

    cfg->flags  = AUDIO_CFG_STEREO | (is_i2s_enabled ? AUDIO_CFG_I2S : AUDIO_CFG_PWM);
    cfg->sample_freq = 44100;
    cfg->cb     = audio_cb;
    cfg->volume = 32767;            // use full range
    cfg->dma_buffer     = audiobuf;
    cfg->buffer_size    = AUDIO_BUFFER_SIZE; 
    cfg->dma_channels[0] = dma_claim_unused_channel(true);
    cfg->dma_channels[1] = dma_claim_unused_channel(true);
    cfg->dma_irq        = DMA_IRQ_1;
    if (is_i2s_enabled) {
        cfg->i2s.data_pin = I2S_DATA_PIO;
        cfg->i2s.bclk_pin = I2S_BCK_PIO;
        cfg->i2s.pio      = pio1,
        cfg->i2s.sm       = 0;
    } else {
        cfg->pwm.base_pin = PWM_PIN0;
    }

    int rtn = audio_init();
    if (rtn != AUDIO_ERR_OK) {
        Sys_Error("audio_init() error %d\n", rtn);
    }

    audio_start();
}

