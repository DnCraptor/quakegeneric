#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include <pico.h>
#include <hardware/pwm.h>
#include <pico/multicore.h>

#include "audio.h"
#include "mixer.h"
#include "quakedef.h"

#if DVI_HSTX
#include "dvi_defs.h"
#endif


extern "C" qboolean CDAudio_GetSamples(int16_t* buf, size_t n);
extern "C" qboolean S_GetSamples(int16_t* buf, size_t n);

// ---------------------------------------------------
mutex_t snd_mutex;		// used to coordinate access between core 1 mixer and core0


#define AUDIO_BUFFER_SIZE_LOG2 9
#define AUDIO_BUFFER_SIZE      (1 << AUDIO_BUFFER_SIZE_LOG2)

static int16_t audiobuf[AUDIO_BUFFER_SIZE * 2];
static int32_t sfxbuf[((AUDIO_BUFFER_SIZE/SFX_DOWNSAMPLE_RATIO + 1) * 2)]; // sound effects buffer
static uint32_t timestamp = 0;
bool is_i2s_enabled = false;

extern "C" qboolean CDAudio_GetSamples(int16_t* buf, size_t n); // FIXME!!
extern "C" void S_RenderSfx(int32_t *sfxbuf, int frames, uint32_t timestamp);

int __not_in_flash_func(audio_cb_common)(int16_t* dst, uint32_t frames, int volscale, int volbias) {
    if (frames == 0) return 0;
    
    // get CD samples (todo: do this another way)
    if (!CDAudio_GetSamples(dst, frames)) memset(dst, 0, sizeof(int16_t) * 2 * frames);
    S_RenderSfx(sfxbuf + 2, frames / SFX_DOWNSAMPLE_RATIO, timestamp / SFX_DOWNSAMPLE_RATIO);
    timestamp += frames;

    // upsample sfx buffer, mix with CD audio and clamp
    // TODO: linearly interpolate sfx buffer data

    int sfxvolume = cvar_volume.value * 32767.0f;
    register int sfx_l, sfx_r, dsfx_l, dsfx_r, dst_l, dst_r;
    int32_t *sfx = sfxbuf;
    do {
        sfx_l  = (sfxvolume * sfx[0]) >> 15;
        sfx_r  = (sfxvolume * sfx[1]) >> 15;
        dsfx_l = (((sfxvolume * sfx[2]) >> 15) - sfx_l) >> 2;
        dsfx_r = (((sfxvolume * sfx[3]) >> 15) - sfx_r) >> 2;

        dst_l  = dst[0] + sfx_l;
        dst_r  = dst[1] + sfx_r;
        dst_l  = MIN(MAX(dst_l, -32768), 32767);
        dst_r  = MIN(MAX(dst_r, -32768), 32767);
        dst[0] = (volscale * (dst_l + volbias)) >> 15;
        dst[1] = (volscale * (dst_r + volbias)) >> 15;

        sfx_l += dsfx_l;
        sfx_r += dsfx_r;
        dst_l  = dst[2] + sfx_l;
        dst_r  = dst[3] + sfx_r;
        dst_l  = MIN(MAX(dst_l, -32768), 32767);
        dst_r  = MIN(MAX(dst_r, -32768), 32767);
        dst[2] = (volscale * (dst_l + volbias)) >> 15;
        dst[3] = (volscale * (dst_r + volbias)) >> 15;

        sfx_l += dsfx_l;
        sfx_r += dsfx_r;
        dst_l  = dst[4] + sfx_l;
        dst_r  = dst[5] + sfx_r;
        dst_l  = MIN(MAX(dst_l, -32768), 32767);
        dst_r  = MIN(MAX(dst_r, -32768), 32767);
        dst[4] = (volscale * (dst_l + volbias)) >> 15;
        dst[5] = (volscale * (dst_r + volbias)) >> 15;

        sfx_l += dsfx_l;
        sfx_r += dsfx_r;
        dst_l  = dst[6] + sfx_l;
        dst_r  = dst[7] + sfx_r;
        dst_l  = MIN(MAX(dst_l, -32768), 32767);
        dst_r  = MIN(MAX(dst_r, -32768), 32767);
        dst[6] = (volscale * (dst_l + volbias)) >> 15;
        dst[7] = (volscale * (dst_r + volbias)) >> 15;

        sfx    += 2;
        dst    += 2*SFX_DOWNSAMPLE_RATIO;
        frames -= SFX_DOWNSAMPLE_RATIO;
    } while (frames);

    sfxbuf[0] = sfxbuf[((AUDIO_BUFFER_SIZE/SFX_DOWNSAMPLE_RATIO)*2)+0];
    sfxbuf[1] = sfxbuf[((AUDIO_BUFFER_SIZE/SFX_DOWNSAMPLE_RATIO)*2)+1];
    return 0;
}

int __not_in_flash_func(audio_cb_hdmi)(int16_t* dst, uint32_t frames, void* priv) {
    return audio_cb_common(dst, frames, (uint32_t)priv, 0);
}

int __not_in_flash_func(audio_cb)(int16_t* dst, uint32_t frames, void* priv, const audio_config_t *cfg) {
    return audio_cb_common(dst, frames, cfg->volscale, cfg->flags & AUDIO_CFG_PWM ? 32768 : 0);
}

void mixer_init(int volume, int is_hdmi) {
    audio_config_t *cfg = audio_init_default_cfg();

    cfg->flags  = AUDIO_CFG_STEREO | (is_i2s_enabled ? AUDIO_CFG_I2S : AUDIO_CFG_PWM);
    cfg->sample_freq = 44100;
    cfg->cb     = audio_cb;
    cfg->volume = (32767 * volume) / 100;
    cfg->dma_buffer     = audiobuf;
    cfg->buffer_size    = AUDIO_BUFFER_SIZE; 
    cfg->dma_channels[0] = dma_claim_unused_channel(true);
    cfg->dma_channels[1] = dma_claim_unused_channel(true);
    cfg->dma_irq        = DMA_IRQ_1;
    if (is_i2s_enabled) {
        cfg->i2s.data_pin = I2S_DATA_PIO;
        cfg->i2s.bclk_pin = I2S_BCK_PIO;
        cfg->i2s.pio      = pio1,
        cfg->i2s.sm       = pio_claim_unused_sm(pio1, false);
    } else {
        cfg->pwm.base_pin = PWM_PIN0;
    }

#if DVI_HSTX
    if (is_hdmi) {
        hdmi_audio_set_buffer(audiobuf, AUDIO_BUFFER_SIZE);
        hdmi_audio_set_cb(audio_cb_hdmi, (void*)cfg->volume);
    } else
#endif
    {
        int rtn = audio_init();
        if (rtn != AUDIO_ERR_OK) {
            Sys_Error("audio_init() error %d\n", rtn);
        }

        audio_start();
    }
}

