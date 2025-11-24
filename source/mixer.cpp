#include <stdint.h>
#include <stdbool.h>

#include <pico.h>
#include <hardware/pwm.h>

#include "audio.h"
#include "mixer.h"

static i2s_config_t i2s_config = {
		.sample_freq = 44100, 
		.channel_count = 2,
        .data_pin = I2S_DATA_PIO,
        .bck_pin = I2S_BCK_PIO,
        .lck_pin = I2S_LCK_PIO,
		.pio = pio1,
		.sm = 0,
        .dma_channel = 0,
        .dma_trans_count = 1,
        .dma_buf = NULL,
        .volume = 0,
        .program_offset = 0
	};

static void PWM_init_pin(uint8_t pinN, uint16_t max_lvl) {
    pwm_config config = pwm_get_default_config();
    gpio_set_function(pinN, GPIO_FUNC_PWM);
    pwm_config_set_clkdiv(&config, 1.0);
    pwm_config_set_wrap(&config, max_lvl); // MAX PWM value
    pwm_init(pwm_gpio_to_slice_num(pinN), &config, true);
}

void mixer_init() {
    if (is_i2s_enabled) {
        i2s_volume(&i2s_config, 0);
    } else {
        PWM_init_pin(PWM_PIN0, (1 << 8) - 1);
        PWM_init_pin(PWM_PIN1, (1 << 8) - 1);
    }
}

void __not_in_flash() mixer_samples(int16_t* samples, size_t n) {
    for (int i = 0; i < n; ++i, samples += 2) {
        if (is_i2s_enabled) {
            i2s_dma_write(&i2s_config, samples);
        } else {
            pwm_set_gpio_level(PWM_PIN1, (uint16_t)((int32_t)samples[0] + 0x8000L) >> 4);
            pwm_set_gpio_level(PWM_PIN0, (uint16_t)((int32_t)samples[1] + 0x8000L) >> 4);
        }
    }
}
