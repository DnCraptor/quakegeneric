# quakegeneric for RP2350

![a low-resolution screenshot of quake](./.github/quakegeneric.png)

it's like [doomgeneric](https://github.com/ozkl/doomgeneric), but for quake. it's based on the GPL WinQuake source code.

currently it can only compile for 32-bit architechtures.

## original

https://github.com/erysdren/quakegeneric (for MS-DOS / SDL2 / Win32)

## platforms supported

ARM Cortex-M33 RP2350 + 8MB QSPI PSRAM

## boards supported

- Murmulator 1.x
- Murmulator 2.0

## video-out supported

- HDMI (without sound)
- VGA

## audio-out supported

- PWM
- i2s TDA1387 / PCM510x

## SD-Card

- /quake folder is required
- /quake/ID1 folder is required (copy it from your copy of Quake distribution)
- /quake/CD folder to store CD-tracks in format
- /quake/argv.conf - use it to populate command line arguments, like -hipnotic or -rouge
- /quake/quake.conf - to tune up performance / overclocking
- https://t.me/ZX_MURMULATOR/241810/244194 https://t.me/murmulator_talks/990 https://t.me/murmulator_news/1668 examples.

## overclocking

/quake/quake.conf example:<br/>
```
CPU=504
FLASH=100
PSRAM=100
VREG=21
```

VREG - is not voltage, but one of enum values. 19 - 1.55V, 20 - 1.6V, 21 - 1.65V...

FLASH/PSRAM - are not actual frequencies, but a limit for autoconfiguration mode.

CPU - RP2350 CPU cores freq. in MHzz

Also supported:

```
FLASH_T=60007304
PSRAM_T=607C6304
```
 - both are hexidecimal masks to set exact dividers (let review RP2350 datasheets)

## License

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
