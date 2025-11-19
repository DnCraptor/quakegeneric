#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
