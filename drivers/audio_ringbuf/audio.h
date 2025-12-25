#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <hardware/pio.h>
#include <hardware/dma.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AUDIO_CFG_I2S           = (0 << 0),
    AUDIO_CFG_PWM           = (1 << 0),

    AUDIO_CFG_STEREO        = (0 << 1),
    AUDIO_CFG_MONO          = (1 << 1),

    AUDIO_CFG_IRQ_SHARED    = (0 << 2),     // use shared IRQ handler
    AUDIO_CFG_IRQ_EXCLUSIVE = (1 << 2),     // use exclusive IRQ handler

    AUDIO_CFG_I2S_GPIO_OFFSET = (1 << 3),   // set if PIO GPIO mapping is offset by 16 (i.e. for RP2350B's GPIO32-47)
};

typedef struct audio_struct_t audio_config_t;
typedef int (*audio_cb_t)(int16_t* dst, uint32_t samples, void* priv, const audio_config_t *cfg);

typedef struct audio_struct_t {
    uint32_t    flags;              // see AUDIO_CFG_*
    uint32_t    sample_freq;        // in hz
    int16_t     volscale;           // plain volume for I2S, prescaler for PWM, 1.15fx
    int16_t     volume;             // regular volume [0..32767], set via audio_set_volume()
    union {
        struct {
            uint8_t base_pin;       // base GPIO for the PWM output at [base_pin, base_pin^1]
            uint8_t slice;          // determined from base pin
            int16_t max_level;      // used for PWM
        } pwm;
        struct {
            PIO     pio;            // null if find the best fit
            uint8_t data_pin;       // data pin
            uint8_t bclk_pin;       // bclk pin, lrclk = blck_bin+1
             int8_t sm;             // state machine number or -1 if find the best fit
             int8_t prog_ofs;       // program offset, automatically filled in
        } i2s;
    };
    
    uint8_t     dma_channels[2];    // two dma channels allocated
    uint8_t     dma_irq;            // irq line
    uint8_t     dma_irq_priority;

    int16_t*    dma_buffer;         // pointer to the DMA buffer
    uint32_t    buffer_size;        // buffer size

    audio_cb_t  cb;                 // callback function pointer
    void*       cb_priv;            // private callback data

    uint16_t    buffer_half_smps;       // size of half of buffer in samples (that is, twice the buffer_half_size for stereo)
    uint16_t    buffer_half_size;       // size of half of buffer
    uint32_t    buffer_offset;          // render offset
    uint32_t    samples_rendered;       // number of samples played
    uint32_t    samples_played;         // wall clock of total samples played
    uint32_t    samples_played_last;    // last queried position (anti-rollback)
} audio_config_t;

enum {
    AUDIO_ERR_OK                = 0,
    AUDIO_ERR_UNKNOWN           = 1,
    AUDIO_ERR_NULLPTR           = 2,
    AUDIO_ERR_INVALID_PARAMS    = 3,
    AUDIO_ERR_OUT_OF_PIO_SM     = 4,
};

// ------------------------

// returns default configuration, to be further filled by the user
audio_config_t* audio_init_default_cfg();

// init the audio driver using the config
int audio_init();

// start the playback
void audio_start();

// pause the playback
void audio_pause();

// stop the playback
void audio_pause();

// set volume (0 - min, 32767 - max)
void audio_set_volume(int16_t volume);

// get current volume
int audio_get_volume();

// stop playback and cleanup
void audio_done();

// return current position in samples
uint32_t audio_get_position();


#ifdef __cplusplus
}
#endif
