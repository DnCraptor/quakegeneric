#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// start of new frame - allocate memory for edges, prefetch vis arrays
void RC_NewFrame();

// abort pending transfers
void RC_AbortNewFrame();

// wait for preload end
void RC_WaitForPreloadEnd();

// end of frame
void RC_EndFrame();

#ifdef __cplusplus
}
#endif
