#include "psram_alloc.h"
#include "quakedef.h"
#include "sys.h"

static uint8_t* base = (uint8_t*)__PSRAM_NEXT;
static uint8_t lock = 0;

void* free_base(void) {
	Sys_Printf("free_base() %ph\n", base);
	lock = 0;
}

void* alloc_base(const char* for_what) {
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
	Sys_Printf("alloc(%d, %s) %ph -> %ph\n", sz, for_what, res, base);
	return res;
}
