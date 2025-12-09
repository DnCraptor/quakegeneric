#include "psram_alloc.h"
#include "quakedef.h"
#include "sys.h"

static uint8_t* base = (uint8_t*)__PSRAM_NEXT;
static uint8_t lock = 0;

void* free_base(void) {
	Sys_Printf("free_base() %ph\n", base);
	lock = 0;
}

void psram_sections_init()
{
	memcpy(&__psram_data_start__, &__psram_data_load__, (&__psram_data_end__ - &__psram_data_start__));
	memset(&__psram_bss_start__, 0, (&__psram_bss_end__ - &__psram_bss_start__));
}

void *alloc_base(const char *for_what)
{
    Sys_Printf("alloc_base(%s) %ph\n", for_what, base);
	if (base > get_sp()) {
		Sys_Printf("WARN! PSRAM alloc_base crosses core0 stack: %ph\n", get_sp());
	}
	lock = 1;
	return base;
}

void* alloc_base_sz(unsigned int sz, const char* for_what) {
	Sys_Printf("alloc_base_sz(%d, %s) %ph\n", sz, for_what, base);
	if (base + sz > get_sp()) {
		Sys_Printf("WARN! PSRAM alloc crosses core0 stack: %ph over SP %ph\n", base + sz, get_sp());
	}
	lock = 1;
	return base;
}

void* alloc(unsigned int sz, const char* for_what) {
	sz = (sz + 3) & ~3; // align by 4
	uint8_t* res = base;
	base += sz;
	if (lock) {
		Sys_Printf("WARN! PSRAM alloc(%d, %s) crosses alloc_base\n", sz, for_what);
	}
	if (base > get_sp()) {
		Sys_Printf("WARN! PSRAM alloc(%d, %s) crosses core0 stack: %ph over SP %ph\n", sz, for_what, base, get_sp());
	}
	Sys_Printf("alloc(%d, %s) %ph -> %ph (sp: %ph)\n", sz, for_what, res, base, get_sp());
	return res;
}

// call a function on a temporary stack
void __attribute__((naked)) stackcall(void (*proc)(), void *new_sp) {
    __asm__ volatile (
        "push   {r4, lr}" "\n"
        "bic    r1, %[new_sp], 7" "\n"    // align by 8 bytes
        "mrs    r4, msp" "\n"
        "msr    msp, r1" "\n"
        "isb" "\n"
        "blx    %[proc]" "\n"
        "msr    msp, r4" "\n"
        "isb" "\n"
        "pop    {r4, pc}" "\n"
        :
        : [new_sp] "r" (new_sp),
          [proc] "r" (proc)
        : "memory"
    );
}
