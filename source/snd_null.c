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
// snd_null.c -- include this instead of all the other snd_* files to have
// no sound code whatsoever

#include "quakedef.h"

cvar_t volume = {"volume", "0.7", true, false, 0.7};

#undef Con_Printf
#define Con_Printf(...)
 
void S_Init (void)
{
	Con_Printf("S_Init\n");
}

void S_AmbientOff (void)
{
	Con_Printf("S_AmbientOff\n");
}

void S_AmbientOn (void)
{
	Con_Printf("S_AmbientOn\n");
}

void S_Shutdown (void)
{
	Con_Printf("S_Shutdown\n");
}

void S_TouchSound (char *sample)
{
	Con_Printf("S_TouchSound %s\n", sample);
}

void S_ClearBuffer (void)
{
	Con_Printf("S_ClearBuffer\n");
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	Con_Printf("S_StaticSound %s\n", sfx->name);
}

void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation)
{
	Con_Printf("S_StartSound [%d:%d] %s\n", entnum, entchannel, sfx->name);
}

void S_StopSound (int entnum, int entchannel)
{
	Con_Printf("S_StopSound [%d:%d]\n", entnum, entchannel);
}

sfx_t* S_PrecacheSound (char *sample)
{
	Con_Printf("S_PrecacheSound %s\n", sample);
	sfx_t* res = Hunk_AllocName(sizeof(sfx_t), sample);
	strncpy(res->name, sample, sizeof(res->name));
	return res;
}

void S_ClearPrecache (void)
{
	Con_Printf("S_ClearPrecache\n");
}

void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{	
	Con_Printf("S_Update\n");
}

void S_StopAllSounds (qboolean clear)
{
	Con_Printf("S_StopAllSounds %d\n", clear);
}

void S_BeginPrecaching (void)
{
	Con_Printf("S_BeginPrecaching\n");
}

void S_EndPrecaching (void)
{
	Con_Printf("S_EndPrecaching\n");
}

void S_ExtraUpdate (void)
{
	Con_Printf("S_ExtraUpdate\n");
}

void S_LocalSound (char *s)
{
	Con_Printf("S_LocalSound %s\n", s);
}

