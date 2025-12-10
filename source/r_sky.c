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
// r_sky.c

#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"


__psram_data("r_sky") int		iskyspeed = 8;
__psram_data("r_sky") int		iskyspeed2 = 2;
__psram_bss ("r_sky") float	skyspeed, skyspeed2;

					  float		skytime;

__psram_bss ("r_sky") byte		*r_skysource;

					  qboolean  r_skymade;
__psram_bss ("r_sky") int 		r_skydirect;		// not used?


// TODO: clean up these routines

//__psram_bss ("r_sky") __aligned(8) byte	bottomsky[128*128];
//__psram_bss ("r_sky") __aligned(8) byte	bottommask[128*128];
//__psram_bss ("r_sky") __aligned(8) uint16_t	bottomsky[128*128*2]; // [sky,mask] interleaved
__psram_bss ("r_sky") __aligned(8) byte topsky[128*128];
__psram_bss ("r_sky") __aligned(8) byte	bottomsky[128*128]; // idx 255 is transparent

byte	newsky[128*128];

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
	int			i, j;
	byte		*src;

	src = (byte *)mt + mt->offsets[0];

	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			topsky[(i*128) + j] = src[i*256 + j + 128];
		}
	}

	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			if (src[i*256 + (j & 0x7F)])
			{
				bottomsky[(i*128) + j] = src[i*256 + (j & 0x7F)];
			}
			else
			{
				bottomsky[(i*128) + j] = 0xFF;
			}
		}
	}
	
	r_skysource = newsky;
}


/*
=================
R_MakeSky
=================
*/
void __no_inline_not_in_flash_func(R_MakeSky) (void)
{
	int			x, y;
	int			ofs, baseofs;
	int			xshift, yshift;
	byte		*pnewsky, *ptopsky;
	static int	xlast = -1, ylast = -1;

	xshift = skytime*skyspeed;
	yshift = skytime*skyspeed;

	if ((xshift == xlast) && (yshift == ylast))
		return;

	xlast = xshift;
	ylast = yshift;
	
	pnewsky = newsky;
	ptopsky = topsky;

	for (y=0 ; y<SKYSIZE ; y++)
	{
		baseofs = ((y+yshift) & SKYMASK) * 128;

		#pragma GCC unroll 4
		for (x=0 ; x<SKYSIZE ; x++)
		{
			ofs = baseofs + ((x+xshift) & SKYMASK);
			*pnewsky++ = bottomsky[ofs] == 255 ? (*ptopsky) : bottomsky[ofs];
			ptopsky++;
		}
	}

	r_skymade = 1;
}


/*
=================
R_GenSkyTile
=================
*/
void R_GenSkyTile (void *pdest)
{
	return;			// not used iirc
#if 0
	int			x, y;
	int			ofs, baseofs;
	int			xshift, yshift;
	unsigned	*pnewsky;
	unsigned	*pd;

	xshift = skytime*skyspeed;
	yshift = skytime*skyspeed;

	pnewsky = (unsigned *)&newsky[0];
	pd = (unsigned *)pdest;

	for (y=0 ; y<SKYSIZE ; y++)
	{
		baseofs = ((y+yshift) & SKYMASK) * 131;

		for (x=0 ; x<SKYSIZE ; x++)
		{
			ofs = baseofs + ((x+xshift) & SKYMASK);

			*(byte *)pd = (*((byte *)pnewsky + 128) &
						*(byte *)&bottommask[ofs]) |
						*(byte *)&bottomsky[ofs];
			pnewsky = (unsigned *)((byte *)pnewsky + 1);
			pd = (unsigned *)((byte *)pd + 1);
		}

		pnewsky += 128 / sizeof (unsigned);
	}
#endif
}

/*
=============
R_SetSkyFrame
==============
*/
void R_SetSkyFrame (void)
{
	int		g, s1, s2;
	float	temp;

	skyspeed = iskyspeed;
	skyspeed2 = iskyspeed2;

	g = GreatestCommonDivisor (iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = SKYSIZE * s1 * s2;

	skytime = cl.time - ((int)(cl.time / temp) * temp);
	

	r_skymade = 0;
}


