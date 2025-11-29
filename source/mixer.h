#pragma once
#ifndef __MIXER__
#define __MIXER__

void mixer_init();
void mixer_samples(int16_t*, size_t);
void mixer_tick();

#endif // __MIXER__
