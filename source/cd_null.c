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
#include "quakedef.h"

static FIL* play_file = 0;
static qboolean play_looping = 0;
static qboolean play_paused = 0;

void CDAudio_Play(byte track, qboolean looping)
{
	if (play_file) {
		f_close(play_file);
	} else {
		play_file = malloc(sizeof(FIL));
	}
	char b[64];
	snprintf(b, 64, "/QUAKE/CD/out%02d.cdr", track);
	Con_Printf("CDAudio_Play %s %s\n", b, looping ? "(in a loop)" : "");
	play_looping = looping;
	play_paused = 0;
	if (f_open(play_file, b, FA_READ) != FR_OK) {
		
	}
	// TODO: prebuf
}


void CDAudio_Stop(void)
{
	if (play_file) {
		Con_Printf("CDAudio_Stop\n");
		f_close(play_file);
		play_file = 0;
	}
}


void CDAudio_Pause(void)
{
	Con_Printf("CDAudio_Pause\n");
	play_paused = 1;
}


void CDAudio_Resume(void)
{
	Con_Printf("CDAudio_Resume\n");
	play_paused = 0;
}


void CDAudio_Update(void)
{
}


int CDAudio_Init(void)
{
	return 0;
}


void CDAudio_Shutdown(void)
{
	CDAudio_Stop();
}
