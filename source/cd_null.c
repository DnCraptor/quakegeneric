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

#define CD_BUF_SIZE 44100

static FIL* play_file = 0;
static qboolean play_looping = 0;
static volatile qboolean play_paused = 0;
static uint8_t cd_buf[CD_BUF_SIZE] __psram_bss("cd_buf");
static size_t pos = 0;
static qboolean invalidated[2] = { 0, 0 };

cvar_t bgmvolume = {"bgmvolume", "0.7", true, false, 0.7};

void CDAudio_Play(byte track, qboolean looping)
{
	CDAudio_Init();
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
	if (!play_file || play_paused)
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

#define samples_per_buffer (CD_BUF_SIZE / 4)        // 11025
#define samples_per_half   (samples_per_buffer / 2) // 5512
// вызывается со второго ядра RP2350 CPU, n == 1 (возможно, позже будет больше и вызов реже)
qboolean __not_in_flash_func() CDAudio_GetSamples(int16_t* buf, size_t n)
{
    while (n--)
    {
        if (!play_file || play_paused)
            return 0;

        size_t half = (pos < samples_per_half) ? 0 : 1;
        if (invalidated[half])
            return 0;

        size_t b_pos = pos * 4;
        int16_t* src = (int16_t*)(cd_buf + b_pos);
		float f = bgmvolume.value;
        buf[0] += src[0] * f;
        buf[1] += src[1] * f;
        buf += 2;

        pos++;

        if (pos == samples_per_half)
            invalidated[0] = 1;
        else if (pos == samples_per_buffer)
        {
            invalidated[1] = 1;
            pos = 0;
        }
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
	return 0;
}


void CDAudio_Shutdown(void)
{
	CDAudio_Stop();
}
