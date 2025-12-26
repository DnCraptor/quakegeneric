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
static uint32_t play_file_data_offset = 0;
static uint32_t play_file_data_end    = 0;
static qboolean play_looping = 0;
static volatile qboolean play_paused = 0;
static uint8_t cd_buf[CD_BUF_SIZE] __psram_bss("cd_buf");
static size_t pos = 0;
static qboolean invalidated[2] = { 0, 0 };
static qboolean initialized = 0;
static qboolean enabled = 0;
static __psram_bss("cd_null") uint8_t remap[256];

cvar_t bgmvolume = {"bgmvolume", "0.5", true};

static int ValidateChunk(uint32_t fourcc, const char* expected) {
    return fourcc == *(uint32_t*)expected;
}

static qboolean CDAudio_PlayAsWaveFile(FIL *f, const char *fname, uint32_t *data_start, uint32_t *data_end) {
	FRESULT fr;
    UINT br;
	struct riff_cunk_t {
		uint32_t fourcc;
		uint32_t size;
	} chunk;
	struct {
		uint32_t 	riff_header;
		uint32_t    riff_size;
		uint32_t    wave_fourcc;
	} header;
	struct fmt_header {
		uint16_t    wFormatTag;         // Format code
		uint16_t    nChannels;          // Number of interleaved channels
		uint32_t    nSamplesPerSec;     // Sampling rate (blocks per second)
		uint32_t    nAvgBytesPerSec;    // Data rate
		uint16_t    nBlockAlign;        // Data block size (bytes)
		uint16_t    wBitsPerSample;     // Bits per sample
	} fmt;
	int fmt_found = 0;

	// open file
	if (f_open(f, fname, FA_READ) != FR_OK) goto fail;

	// read RIFF header and validate it
    fr = f_read(f, &header, sizeof(header), &br);
    if (fr != FR_OK || br != 12) goto fail;
    if (!ValidateChunk(header.riff_header, "RIFF") || !ValidateChunk(header.wave_fourcc, "WAVE")) goto fail;

	while (!f_eof(f)) {
        fr = f_read(f, &chunk, sizeof(chunk), &br);
        if (fr != FR_OK || br != sizeof(chunk)) goto fail;

		// check if "fmt " chunk
		if (ValidateChunk(chunk.fourcc, "fmt ")) {
            if (chunk.size < 16) goto fail;

			// read format data
			fr = f_read(f, &fmt, sizeof(fmt), &br);
        	if (fr != FR_OK || br != sizeof(fmt)) goto fail;

			Sys_Printf("%d %d %d %d\n", fmt.nChannels, fmt.nSamplesPerSec, fmt.wBitsPerSample, fmt.wFormatTag);

			// verify it's PCM 16 bits stereo 44100 Hz
			if (fmt.nChannels != 2 || fmt.nSamplesPerSec != 44100 || fmt.wBitsPerSample != 16 || fmt.wFormatTag != 1) goto fail;

			// found! skip the residual data
			fmt_found = 1;
			fr = f_lseek(f, f->fptr + (chunk.size - sizeof(fmt)));
			if (fr != FR_OK) goto fail;
		}
		else if (ValidateChunk(chunk.fourcc, "data")) { 
			// data chunk
			if (fmt_found == 0) goto fail;		// data before format!
			if (data_start != NULL) *data_start = (DWORD)f_tell(f);
			if (data_end   != NULL) *data_end   = *data_start + chunk.size;

			// valid .wav file - return success, leave file open
			return 1;
		} else {
			// unknown chunk, skip
			fr = f_lseek(f, f->fptr + chunk.size);
			if (fr != FR_OK) goto fail;
		}
	}

	// cleanup if failed
fail:
	f_close(f);
	return 0;
}

void CDAudio_Play(byte track, qboolean looping)
{
	if (!initialized || !enabled) return;
	if (play_file) {
		f_close(play_file);
	} else {
		play_file = malloc(sizeof(FIL));
	}
	play_paused = 1;
	pos = 0;

	char b[22];

	// first try to open as .wav file
	snprintf(b, 22, "/QUAKE/CD/track%02d.wav", track);
	if (CDAudio_PlayAsWaveFile(play_file, b, &play_file_data_offset, &play_file_data_end) == 0) {
		// now try opening as raw PCM track
		play_file_data_offset = 0;
		play_file_data_end   = -1;  // until file end
		snprintf(b, 22, "/QUAKE/CD/out%02d.cdr", track);
		if (f_open(play_file, b, FA_READ) != FR_OK) {
			free (play_file); play_file = NULL;
			return;
		}
	}

	play_looping = looping;
	play_paused = 0;
	Con_Printf("CDAudio_Play %s %s\n", b, looping ? "(in a loop)" : "");
	play_paused = !CDAudio_GetPCM(cd_buf, CD_BUF_SIZE);
	invalidated[0] = 0;
	invalidated[1] = 0;
}

qboolean CDAudio_GetPCM(unsigned char* buf, size_t len)
{
	if (!initialized || !enabled || !play_file || play_paused)
		return 0;

	UINT br = 0;

	do {
		// determine how much to read
		UINT len_now = MIN(len, (play_file_data_end - f_tell(play_file)));
		
		// read :)
		FRESULT fr = f_read(play_file, buf, len_now, &br); if (fr != 0) goto end_of_file;

		// read less than requested or end of file (in case of .wav)
		if (br < len)
		{
			if (play_looping)
			{
				// seek to start of audio data
				if (f_lseek(play_file, play_file_data_offset) != FR_OK) goto end_of_file; 
			} else {
end_of_file:
				// fill the rest with silence, close file and return 0;
				memset(buf + br, 0, len - br);
				CDAudio_Stop();
				return 0;
			}
		}
		len -= br;
		buf += br;
	} while (len > 0);

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

#define samples_per_buffer (CD_BUF_SIZE / 4)       
#define samples_per_half   (samples_per_buffer / 2)

// called by audio callback from core#1
qboolean __not_in_flash_func() CDAudio_GetSamples(int16_t* buf, size_t n)
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
		//Con_Printf("CDAudio_Stop\n");
		f_close(play_file);
		free(play_file);
		play_file = 0;
		play_file_data_offset = 0;
		play_file_data_end    = -1;
	}
	invalidated[0] = 1;
	invalidated[1] = 1;
}


void CDAudio_Pause(void)
{
	if (!initialized || !enabled) return;
	//Con_Printf("CDAudio_Pause\n");
	play_paused = 1;
}


void CDAudio_Resume(void)
{
	if (!initialized || !enabled) return;
	//Con_Printf("CDAudio_Resume\n");
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
