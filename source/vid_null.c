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

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];

#ifndef ZBUFFER_IN_SRAM
short*	zbuffer = (short*)__PSRAM_Z_BUFF;
#else
short zbuffer[BASEWIDTH*BASEHEIGHT]; // 153600 = 0x25800
#endif
byte	surfcache[652800] __psram_bss("surfcache");
size_t	surfcache_size = 652800;

//#define SURFCACHE_IN_SRAM
#ifdef  SURFCACHE_IN_SRAM
#define SURFCACHE_SRAM_SIZE (256 * 1024)
static byte surfcache_sram[SURFCACHE_SRAM_SIZE];
#endif

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

	surfcache_size = 652800; // D_SurfaceCacheForRes(BASEWIDTH, BASEHEIGHT); // 652800
	if (!surfcache) {
#ifndef SURFCACHE_IN_SRAM
		//surfcache_size = 652800; // D_SurfaceCacheForRes(BASEWIDTH, BASEHEIGHT); // 652800
		/// TODO: may be moved to SRSAM?
		//surfcache = (byte*)alloc(surfcache_size, "surfcache");
#else
		surfcache_size = SURFCACHE_SRAM_SIZE;
		surfcache = surfcache_sram;
#endif
	}
	D_InitCaches (surfcache, surfcache_size);

	// quake generic
	QG_Init();
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

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


