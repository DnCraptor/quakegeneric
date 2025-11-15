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
// sys_null.h -- null system driver to aid porting efforts

#include "quakedef.h"
#include "errno.h"
#include "ff.h"

qboolean isDedicated;

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             10
static FIL* sys_handles[MAX_HANDLES] = { 0 };

int findhandle (void)
{
	int             i;
	
	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FIL* f = malloc(sizeof(FIL));
	if (!f) {
		*hndl = -1;
		return -1;
	}
	int i = findhandle ();
	if (f_open(f, path, FA_READ) != FR_OK)
	{
		free(f);
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	return f_size(f);
}

int Sys_FileOpenWrite (char *path)
{
	FIL* f = malloc(sizeof(FIL));
	if (!f) {
		Sys_Error ("Error opening %s: %s", path, "NOMEM");
		return -1;
	}
	int i = findhandle ();
	if (f_open(f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		free(f);
		Sys_Error ("Error opening %s: %s", path, "f_open");
		return -1;
	}
	sys_handles[i] = f;
	return i;
}

void Sys_FileClose (int handle)
{
	f_close(sys_handles[handle]);
	free(sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	f_lseek(sys_handles[handle], position);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	UINT rb = 0;
	f_read (dest, sys_handles[handle], count, &rb);
	return rb;
}

int Sys_FileWrite (int handle, void *data, int count)
{
	UINT wb = 0;
	f_write (data, sys_handles[handle], count, &wb);
	return wb;
}

int     Sys_FileTime (char *path)
{
    FILINFO fno;
    if (FR_OK != f_stat(path, &fno)) {
		return -1;
	}
	return ((int)fno.fdate << 16) | fno.ftime;
}

void Sys_mkdir (char *path)
{
	f_mkdir(path);
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}

void Sys_Error (char *error, ...)
{
	va_list         argptr;

	printf ("Sys_Error: ");   
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;
	
	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
	exit (0);
}

double Sys_FloatTime (void)
{
	static double t;
	
	t += 0.1;
	
	return t;
}

char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents (void)
{
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

//=============================================================================

/*

void main (int argc, char **argv)
{
	static quakeparms_t    parms;

	parms.memsize = 8*1024*1024;
	parms.membase = malloc (parms.memsize);
	parms.basedir = ".";

	COM_InitArgv (argc, argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	printf ("Host_Init\n");
	Host_Init (&parms);
	while (1)
	{
		Host_Frame (0.1);
	}
}

*/

void _unlink(const char* path) {
	f_unlink(path);
}
