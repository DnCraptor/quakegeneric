#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// borrow symbols from the linker
extern uint8_t __psram_data_load__;
extern uint8_t __psram_data_start__;
extern uint8_t __psram_data_end__;

extern uint8_t __psram_bss_start__;
extern uint8_t __psram_bss_end__;

extern uint8_t __psram_heap_start__;

// attribute macros to place variables in PSRAM sections

// place initialized data or code in PSRAM
#ifndef __psram_data
#define __psram_data(group) __attribute__((section(".psram_data." group)))
#endif

// place uninitialized data (filled with 0!) in PSRAM
#ifndef __psram_bss
#define __psram_bss(group) __attribute__((section(".psram_bss." group)))
#endif

// init psram_data/psram_bss sections
// MUST be called _after_ PSRAM init but _before_ any psram_data/psram_bss sections are accessed!
void psram_sections_init();

// -------------------------

// no base changes, just validate
void* alloc_base(const char* for_what);
void* alloc_base_sz(unsigned int sz, const char* for_what);
void* free_base(void);
// just shift base down
void* alloc(unsigned int sz, const char* for_what);

static inline unsigned int get_sp(void) {
    unsigned int sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

#ifdef __cplusplus
}
#endif
