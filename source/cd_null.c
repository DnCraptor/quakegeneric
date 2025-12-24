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

#define CD_BUF_SIZE 16384

static FIL* play_file = 0;
static qboolean play_looping = 0;
static volatile qboolean play_paused = 0;
static uint8_t* cd_buf = 0;
static size_t pos = 0;
static qboolean invalidated[2] = { 0, 0 };
static qboolean initialized = 0;
static qboolean enabled = 0;
static __psram_bss("cd_null") uint8_t remap[256];

cvar_t bgmvolume = {"bgmvolume", "0.5", true};

void CDAudio_Play(byte track, qboolean looping)
{
	if (!initialized || !enabled) return;
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
		free (play_file);
		play_file = 0;
		return;
	}
	play_paused = !CDAudio_GetPCM(cd_buf, CD_BUF_SIZE);
	invalidated[0] = 0;
	invalidated[1] = 0;
}

qboolean CDAudio_GetPCM(unsigned char* buf, size_t len)
{
	// Если файла нет или воспроизведение на паузе — данных не будет
	if (!initialized || !enabled || !play_file || play_paused)
		return 0;

	UINT br = 0;

	// читаем данные, даже если мы на EOF — FatFS все равно отдаст, сколько сможет
	FRESULT fr = f_read(play_file, buf, len, &br);
	if (fr != FR_OK)
		return 0;

	// Если прочитано меньше чем нужно
	if (br < len)
	{
		// Лупящийся трек: дочитываем остаток с начала файла
		if (play_looping)
		{
			if (f_lseek(play_file, 0) != FR_OK)
				return 0;

			// дочитываем оставшуюся часть (рекурсивно)
			return CDAudio_GetPCM(buf + br, len - br);
		}

		// Нелупящийся трек: дополняем тишиной и останавливаем
		memset(buf + br, 0, len - br);

		// ВАЖНО: после Stop() файл уничтожен → возврат должен быть 0
		CDAudio_Stop();
		return 0;
	}

	return 1;
}

static void CDAudio_WriteToBuffer(int16_t *dst, int16_t *src, int vol, int frames) {
	if (frames > 0) do {
		dst[0] = (src[0] * vol) >> 15;
        dst[1] = (src[1] * vol) >> 15;
        dst += 2;
		src += 2;
	} while (--frames);
}

#define samples_per_buffer (CD_BUF_SIZE / 4)        // 11025
#define samples_per_half   (samples_per_buffer / 2) // 5512
// вызывается со второго ядра RP2350 CPU, n == 1 (возможно, позже будет больше и вызов реже)
qboolean CDAudio_GetSamples(int16_t* buf, size_t n)
{
	if (!initialized || !enabled || !play_file || play_paused)
            return 0;

	int vol = bgmvolume.value * 32767;
	int half, half_end;
	if (pos < samples_per_half) {
		half = 0;
		half_end = samples_per_half;
	} else {
		half = 1;
		half_end = samples_per_buffer;
	}
	if (invalidated[half]) return 0;
	int16_t* src = (int16_t*)(cd_buf + pos*4);

	// render up to half end or number of samples requested
	int render_now = (half_end - pos); if (render_now > n) render_now = n;
	CDAudio_WriteToBuffer(buf, src, vol, render_now);
	buf += render_now*2;
	src += render_now*2;
	pos += render_now;
	if (pos == half_end) {
		invalidated[half] = 1;
		if (half == 1) { 
			pos = 0;
			src = (int16_t*)(cd_buf);
		}
		half ^= 1;
	}
	if (render_now == n) return 1;

	// render the rest
	render_now = n - render_now;
	CDAudio_WriteToBuffer(buf, src, vol, render_now);
	pos += render_now;

    return 1;
}

void CDAudio_Stop(void)
{
	if (!initialized || !enabled) return;
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
	if (!initialized || !enabled) return;
	Con_Printf("CDAudio_Pause\n");
	play_paused = 1;
}


void CDAudio_Resume(void)
{
	if (!initialized || !enabled) return;
	Con_Printf("CDAudio_Resume\n");
	play_paused = 0;
}


void CDAudio_Update(void)
{
	if (!initialized || !enabled) return;
	if (invalidated[0]) {
		CDAudio_GetPCM(cd_buf, CD_BUF_SIZE / 2);
		invalidated[0] = 0;
	}
	if (invalidated[1]) {
		CDAudio_GetPCM(cd_buf + CD_BUF_SIZE / 2, CD_BUF_SIZE / 2);
		invalidated[1] = 0;
	}
}

static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;
	int		startAddress;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	if (!initialized) {
		Con_Printf("CD Audio not initialized.\n");
		return;
	}

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (play_file)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (play_file)
			CDAudio_Stop();
		for (n = 0; n < 256; n++)
			remap[n] = n;
		
		// nothing else to reset
		return;
	}

	if (Q_strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 256; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi(Cmd_Argv (n+1));
		return;
	}

	if (!enabled)
	{
		Con_Printf("No CD in player.\n");
		return;
	}

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play(Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play(Q_atoi(Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}
}


int CDAudio_Init(void)
{
	int n;

	if (cls.state == ca_dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio")) {
		return -1;
	}

	Cvar_RegisterVariable(&bgmvolume);
	if (!cd_buf) cd_buf = alloc(CD_BUF_SIZE, "cd_buf");

	for (n = 0; n < 256; n++)
		remap[n] = n;

	initialized = true;
	enabled = true;
	
	Cmd_AddCommand ("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}

void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();

	initialized = false;
	enabled = false;
}
