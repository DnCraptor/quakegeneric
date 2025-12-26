#pragma once
#ifndef __MIXER__
#define __MIXER__

#ifdef __cplusplus
extern "C" {
#endif

extern mutex_t snd_mutex;

void mixer_init(int volume);
void mixer_samples(int16_t*, size_t);
void mixer_tick();

#ifdef __cplusplus
}
#endif

#endif // __MIXER__
