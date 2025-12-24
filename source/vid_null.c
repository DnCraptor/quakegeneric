/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"

#include "quakegeneric.h"

viddef_t	vid;				// global video state

#define	BASEWIDTH	320
#define	BASEHEIGHT	240

#define ZBUFFER_IN_SRAM
#define AUX_BUFFER_SIZE (64 * 1024)		// should be "enough"

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];

#ifndef ZBUFFER_IN_SRAM
short*	zbuffer = (short*)__PSRAM_Z_BUFF;
#else
short zbuffer[BASEWIDTH*BASEHEIGHT]; // 153600 = 0x25800
#endif
byte	*surfcache = 0;
size_t	surfcache_size;

//#define SURFCACHE_IN_SRAM
#ifdef  SURFCACHE_IN_SRAM
#define SURFCACHE_SRAM_SIZE (256 * 1024)
static byte surfcache_sram[SURFCACHE_SRAM_SIZE];
#endif

// evil z-buffer allocator, used for overoptimizing certain things ;)
uint8_t *zba_rover;
void ZBA_Reset() {
	zba_rover = (uint8_t*)&zbuffer[BASEWIDTH*BASEHEIGHT];
}
uint8_t* ZBA_GetRover() {
	return zba_rover;
}
void ZBA_FreeToRover(uint8_t *rover) {
	if (((uintptr_t)rover <= (uintptr_t)zbuffer) || ((uintptr_t)rover > (uintptr_t)&zbuffer[BASEWIDTH*BASEHEIGHT])) {
		Sys_Error("ZBA_FreeToRover(): attempt to reset rover outside of Z-buffer memory (%08X != [%08X..%08X])\n",
			(uintptr_t)rover, (uintptr_t)zbuffer, (uintptr_t)&zbuffer[BASEWIDTH*BASEHEIGHT]
		);
	}
	zba_rover = rover;
}

void *ZBA_Alloc(int bytes) {
	if ((uintptr_t)(zba_rover - bytes) <= (uintptr_t)zbuffer) {
		Sys_Error("ZBA_Alloc(): tried to allocate %d bytes but ran out of Z-buffer memory\n", bytes);
		return NULL;
	}
	zba_rover -= bytes;
	//Sys_Printf("ZBA_Alloc(): %d bytes -> 0x%08X", bytes, (uintptr_t)zba_rover);
	return (void*)zba_rover;
}

int ZBA_GetZBufferMaxRow() {
	int rtn = (zba_rover - (uint8_t*)&zbuffer[BASEWIDTH-1]) / (sizeof(short)*BASEWIDTH);
	return rtn < 0 ? 0 : rtn;
}

uint32_t ZBA_GetFreeBytes() {
	return ((uintptr_t)zba_rover - (uintptr_t)zbuffer);
}

// ----------------------
// aux buffer allocation stuff
static __aligned(8) uint8_t auxbuffer[AUX_BUFFER_SIZE];

uint8_t *auxa_rover = auxbuffer;
void AUXA_Reset() {
	auxa_rover = auxbuffer;
}

uint8_t* AUXA_GetRover() {
	return auxa_rover;
}

void AUXA_FreeToRover(uint8_t *rover) {
	if (((uintptr_t)rover < (uintptr_t)auxbuffer) || ((uintptr_t)rover > (uintptr_t)&auxbuffer[AUX_BUFFER_SIZE])) {
		Sys_Error("AUXA_FreeToRover(): attempt to reset rover outside of aux buffer memory (%08X != [%08X..%08X])\n",
			(uintptr_t)rover, (uintptr_t)auxbuffer, (uintptr_t)&auxbuffer[AUX_BUFFER_SIZE]
		);
	}
	auxa_rover = rover;
}

// enforces 8-bytes align
void *AUXA_Alloc(int bytes) {
	if ((uintptr_t)(auxa_rover + bytes + 7) >= (uintptr_t)&auxbuffer[AUX_BUFFER_SIZE]) {
		Sys_Error("AUXA_Alloc(): tried to allocate %d bytes but ran out of aux buffer memory\n", bytes);
		return NULL;
	}
	uint8_t *rtn = (uint8_t*)(((uintptr_t)auxa_rover + 7) & ~7);
	auxa_rover   = (uint8_t*)(((uintptr_t)auxa_rover + bytes + 7) & ~7);
	//Sys_Printf("AUXA_Alloc(): %d bytes -> 0x%08X", bytes, (uintptr_t)rtn);
	return (void*)rtn;
}

// a pair of malloc/free-like functions for local allocations
void *AUXA_MAlloc(int bytes) {
	uint8_t *rover = AUXA_GetRover();
	void *rtn = AUXA_Alloc(bytes + sizeof(uint8_t*));
	*(uint8_t**)rtn = rover;
	return (void*)((uintptr_t)rtn + sizeof(uint8_t*));
}

void AUXA_MFree(void *ptr) {
	// extract rover
	uint8_t *rover = *(uint8_t**)((uintptr_t)ptr - 4);
	AUXA_FreeToRover(rover);
}

uint32_t AUXA_GetFreeBytes() {
	return ((uintptr_t)&auxbuffer[AUX_BUFFER_SIZE] - (uintptr_t)auxa_rover);
}

void	VID_SetPalette (unsigned char *palette)
{
	// quake generic
	QG_SetPalette(palette);
}

void	VID_ShiftPalette (unsigned char *palette)
{
	// quake generic
	QG_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{
	vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;
	
	d_pzbuffer = zbuffer;

	if (!surfcache) {
#ifndef SURFCACHE_IN_SRAM
		surfcache_size = D_SurfaceCacheForRes(BASEWIDTH, BASEHEIGHT); // 652800
		/// TODO: may be moved to SRSAM?
		surfcache = (byte*)alloc(surfcache_size, "surfcache");
#else
		surfcache_size = SURFCACHE_SRAM_SIZE;
		surfcache = surfcache_sram;
#endif
	}
	D_InitCaches (surfcache, surfcache_size);

	// quake generic
	QG_Init();

	// reset Z-buffer allocator
	ZBA_Reset();

	// reset aux allocator
	AUXA_Reset();
}

void	VID_Shutdown (void)
{
	///	free(surfcache);
}

void	VID_Update (vrect_t *rects)
{
	// quake generic
	QG_DrawFrame(vid.buffer);
}

static __psram_bss("vid_null") byte	backingbuf[48*24];

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	int		i, j, reps, repshift;
	vrect_t	rect;

	if (vid.numpages != 1)
		return;

	if (vid.aspect > 1.5)
	{
		reps = 2;
		repshift = 1;
	}
	else
	{
		reps = 1;
		repshift = 0;
	}

	VID_LockBuffer ();
	for (i=0 ; i<(height << repshift) ; i += reps)
		{
			for (j=0 ; j<reps ; j++)
			{
				memcpy (&backingbuf[(i + j) * 24],
						FRAME_BUF+ x + ((y << repshift) + i + j) * vid.rowbytes,
						width);
				memcpy (FRAME_BUF + x + ((y << repshift) + i + j) * vid.rowbytes,
						&pbitmap[(i >> repshift) * width],
						width);
			}
		}
	VID_UnlockBuffer ();
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
	int		i, j, reps, repshift;
	vrect_t	rect;

	if (vid.numpages != 1)
		return;

	if (vid.aspect > 1.5)
	{
		reps = 2;
		repshift = 1;
	}
	else
	{
		reps = 1;
		repshift = 0;
	}

	VID_LockBuffer ();
	for (i=0 ; i<(height << repshift) ; i += reps)
		{
			for (j=0 ; j<reps ; j++)
			{
				memcpy (FRAME_BUF+ x + ((y << repshift) + i + j) * vid.rowbytes,
						&backingbuf[(i + j) * 24],
						width);
			}
		}
	VID_UnlockBuffer ();
}


