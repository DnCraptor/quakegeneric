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
static volatile qboolean play_paused = 0;
static uint8_t* cd_buf = 0;
static size_t pos = 0;
#define CD_BUF_SIZE 44100
static qboolean invalidated[2] = { 0, 0 };

void CDAudio_Play(byte track, qboolean looping)
{
	if (play_file) {
		f_close(play_file);
	} else {
		play_file = malloc(sizeof(FIL));
	}
	char b[22];
	snprintf(b, 22, "/QUAKE/CD/out%02d.cdr", track);
	Con_Printf("CDAudio_Play %s %s\n", b, looping ? "(in a loop)" : "");
	play_looping = looping;
	play_paused = 0;
	pos = 0;
	if (f_open(play_file, b, FA_READ) != FR_OK) {
		play_paused = 1;
		return;
	}
	play_paused = !CDAudio_GetPCM(cd_buf, CD_BUF_SIZE);
	invalidated[0] = 0;
	invalidated[1] = 0;
}

qboolean CDAudio_GetPCM(unsigned char* buf, size_t len) {
	if (!play_file || play_paused || (!play_looping && f_eof(play_file))) return 0;
	UINT br = 0;
	f_read(play_file, buf, len, &br);
	if (br < len) {
		if (play_looping) {
			if (f_lseek(play_file, 0) != FR_OK) return 0;
			return CDAudio_GetPCM(buf + br, len - br);
		} else {
			memset(buf + br, 0, len - br);
			CDAudio_Stop();
		}
	}
	return 1;
}

qboolean CDAudio_GetSamples(int16_t* buf, size_t n) {
	while (n > 0) {
		if (!play_file || play_paused) return 0;
		size_t b_pos = pos << 2;
		if ((b_pos < CD_BUF_SIZE / 2 && invalidated[0]) || (b_pos >= CD_BUF_SIZE / 2 && invalidated[1])) return 0;
		int16_t* src = (int16_t*)(cd_buf + b_pos);
		buf[0] = src[0];
		buf[1] = src[1];
		++pos;
		if (pos == CD_BUF_SIZE / 8) invalidated[0] = 1;
		else if (pos >= CD_BUF_SIZE / 4) { pos = 0; invalidated[1] = 1; }
		buf += 2;
		--n;
	}
	return 1;
}

void CDAudio_Stop(void)
{
	if (play_file) {
		Con_Printf("CDAudio_Stop\n");
		f_close(play_file);
		free(play_file);
		play_file = 0;
	}
	invalidated[0] = 1;
	invalidated[1] = 1;
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
	if (invalidated[0]) {
		CDAudio_GetPCM(cd_buf, CD_BUF_SIZE / 2);
		invalidated[0] = 0;
	}
	if (invalidated[1]) {
		CDAudio_GetPCM(cd_buf + CD_BUF_SIZE / 2, CD_BUF_SIZE / 2);
		invalidated[1] = 0;
	}
}


int CDAudio_Init(void)
{
	if (!cd_buf) cd_buf = alloc(CD_BUF_SIZE, "cd_buf");
	return 0;
}


void CDAudio_Shutdown(void)
{
	CDAudio_Stop();
}
