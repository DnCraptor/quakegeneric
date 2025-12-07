#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int xipstream_init();
int xipstream_start(void *dst, void *src, uint32_t words);
int xipstream_is_running();
int xipstream_wait_blocking();
int xipstream_abort();

#ifdef __cplusplus
}
#endif
