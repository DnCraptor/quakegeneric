# quakegeneric for RP2350 (+8MB PSRAM)

![a low-resolution screenshot of quake](./.github/quakegeneric.png)

it's like [doomgeneric](https://github.com/ozkl/doomgeneric), but for quake. it's based on the GPL WinQuake source code. now ported to the Raspberry Pi RP2350 MCU boards with additional 8MB QSPI PSRAM :)

## features

* supports shareware/registered Quake I .pak's, official mission packs, and third-party addons as well (untested)
* video output (pixel-doubled 320x240 as 640x480 60 Hz) via VGA or DVI/HDMI (auto-detected)
* uses SD card (SPI mode, FAT32/exFAT) for storing game assets
* audio (CD audio tracks + sound effects) output via PWM or external I2S DAC
* PS/2 and USB HID keyboard/mouse support
* NES gamepad support

## supported board configurations:

This port is made around the following board platforms:

* Murmulator 1.x (obsolete)

  ![](https://static.tildacdn.com/tild3565-3534-4538-b863-656164616239/Murmulator_M1.JPG)

    #### QSPI PSRAM CS

    * GPIO 19 (RP2350A) or GPIO 47 (RP2350B)

    #### I2S DAC or PWM audio:

  | GPIO pin | I2S Function | PWM function |
  | -------- | ------------ | ------------ |
  | 26       | DATA         | none         |
  | 27       | BCLK         | PWM_LEFT     |
  | 28       | LRCLK        | PWM_RIGHT    |

    #### DVI/HDMI video output:
  | GPIO pin | VGA function | DVI/HDMI function |
  | -------- | ------------ | ----------------- |
  | 6        | B0           | Clock-            |
  | 7        | B1           | Clock+            |
  | 8        | G0           | Lane0-            |
  | 9        | G1           | Lane0+            |
  | 10       | R0           | Lane1-            |
  | 11       | R1           | Lane1+            |
  | 12       | HSYNC        | Lane2-            |
  | 13       | VSYNC        | Lane2+            |

    #### (micro)SD card slot:
  | GPIO pin | SD Card Function |
  | -------- | ---------------- |
  | 2        | SD_CLK           |
  | 3        | SD_MOSI          |
  | 4        | SD_MISO          |
  | 5        | SD_CS            |

    #### PS/2 Keyboard/Mouse
  | GPIO pin | SD Card Function                      |
  | -------- | ------------------------------------- |
  | 0        | Keyboard_Clock                        |
  | 1        | Keyboard_Data                         |
  | 14       | Mouse_Clock (shared with NES Gamepad) |
  | 15       | Mouse_Data (shared with NES Gamepad)  |

    #### NES Gamepad:
  | GPIO pin | SD Card Function               |
  | -------- | ------------------------------ |
  | 14       | Clock (shared with PS/2 Mouse) |
  | 15       | Latch (shared with PS/2 Mouse) |
  | 16       | Data1                          |

* Murmulator 2.x:
  ![](https://static.tildacdn.com/tild3933-3537-4165-b437-343035333338/Murmulator2_38NJU24_.JPG)

  Key features:

  * HSTX-driven display driver, with lower CPU overhead, also supporting 2197-color (13^3) VGA output over RGB222 pins via high-speed 4-phase PWM.
    * supports custom pin mappings (Pico-DVI-Sock, etc.) with config options.
    * HDMI audio support coming soon!

  #### QSPI PSRAM CS

  * GPIO 8 (RP2350A) or GPIO 47 (RP2350B)

  #### I2S DAC or PWM audio:

  | GPIO pin | I2S Function | PWM function |
  | -------- | ------------ |-------|
  | 9        | DATA         |none|
  | 10       | BCLK         |PWM_LEFT|
  | 11       | LRCLK        |PWM_RIGHT|

  #### DVI/HDMI video output:
  | GPIO pin | VGA function | DVI/HDMI function |
  | -------- | ------------ | ----------------- |
  | 12   | B0 | Clock-                |
  | 13   | B1 | Clock+                |
  | 14   | G0 | Lane0-                |
  | 15   | G1 | Lane0+                |
  | 16   | R0 | Lane1-                |
  | 17   | R1 | Lane1+                |
  | 18   | HSYNC | Lane2-                |
  | 19   | VSYNC | Lane2+                |

  #### (micro)SD card slot:
  | GPIO pin | SD Card Function |
  | -------- | ------------ |
  | 4        | SD_MISO  |
  | 5       | SD_CS    |
  | 6       | SD_CLK  |
  | 7       | SD_MOSI |

  #### PS/2 Keyboard/Mouse
  | GPIO pin | SD Card Function |
  | -------- | ---------------- |
  | 0        | Mouse_Clock      |
  | 1        | Mouse_Data       |
  | 2        | Keyboard_Clock   |
  | 3        | Keyboard_Data    |

  #### NES Gamepad:
  | GPIO pin | SD Card Function |
  | -------- | ---------------- |
  | 20       | Clock            |
  | 21       | Latch            |
  | 26       | Data1            |

Other boards support can be implemented as well by tweaking compile options.

## how to start

You need a RP2350 board with at least 4 MB QSPI Flash and 8MB QSPI PSRAM installed on-board (e.g. Pimoroni Pico Plus 2). Some boards only feature a footprint for the QSPI PSRAM, in that case you can solder a compatible PSRAM chip (e.g. ESP-PSRAM64 or APS6064L) and make sure the CS line is connected to the correct GPIO (refer to a table above). 

If your board doesn't have a QSPI PSRAM, another option is to solder a "sandwich" on top of QSPI Flash chip - orient your PSRAM on top of Flash IC, solder pins 2..8 of PSRAM to the corresponding pins 2..8 of Flash, and connect pin 1 (CS) of PSRAM to a target GPIO. This can be tricky with smaller Flash footprints (as seen on original Pi Pico 2), but can be done with enough patience :)

Next, make sure at least the SD card and VGA/DVI/HDMI output is wired up - same for the additional peripherals.

#### SD Card Contents

Your SD card must be formatted to FAT32 or exFAT. Create a `quake` directory in root of your card, and copy the files necessary:

* `/quake/id1`  - shareware/registered Quake (including `default.cfg` game configuration file)
* `/quake/hipnotic` - Mission Pack I (run with `-hipnotic`)
* `/quake/rogue` - Mission Pack II (run with `-rogue`)
* `/quake/cd` for CD audio tracks (NN=02..99):
  * `/quake/cd/trackNN.wav` - .wav, PCM, 16 bits stereo 44100 Hz, or
  * `/quake/cd/outNN.cdr` - raw PCM 16 bits stereo 44100 Hz
* `/quake/quake.conf` - system configuration file (see below)
* `/quake/argv.conf` - command line parameters (e.g. `-rogue`, `-nosound`, `-nocdaudio`, etc.)

Logs are written to the `/quake.log` file.

By default, quakegeneric uses safe default CPU/Flash/PSRAM timings, which can be overriden by `quake.conf`.

#### overclocking

The RP2350 overclocks well up to 252/378 MHz with safe overvolting (at around 1.35 V), and already provides an acceptable performance (roughly at Pentium 133-166 level). Since most of game and rendering logic is limited by Flash and especially PSRAM memory bandwidth, it's more important to overclock the QSPI bus - unfortunately, it seems to be usually limited at 133-140 MHz.

Most boards can be pushed further and run stable at 504 MHz CPU and 126 MHz Flash/PSRAM, resulting in P233MMX-level performance, but this usually implies 1.6+ V core voltage and hence require an adequate MCU cooling (a heatsink should be enough). See the section below for further info about tweaking

#### additional command line switches (`/quake/argv.conf`)

`-quietlog` - disable non-critical logging to SD card - reduces microstutters.

#### additional CVars

`stacktosram` - due to excessive stack usage, it's placed in PSRAM, resulting in lower performance. This CVar, being set to 1, calls rendering functions on a temporary stack in fast SRAM. This usually improves performance by 5-10% but can result in crashes on large and complex maps. Default = 0

## system configuration file example

`/quake/quake.conf` example:<br/>

```
CPU=504
FLASH=100
PSRAM=100
VREG=21
```

Supported parameters:

* **VREG** - core voltage expressed as `vreg_voltage` enum, e.g. 10 - 1.10V (default), 15 - 1.30 V, 19 - 1.6V, 20 - 1.65V, 21 - 1.7V. Tweaking VREG value improves overclocking potential, but will lead to increased power consumption and higher MCU temperatures.

* **CPU** - CPU frequency in MHz. Expected values are 126, 252, 378 and 504 - others will also work but as video timings are tied to the CPU frequency, it can cause issues with some displays due to higher horizontal/vertical refresh rate.

* **FLASH** - maximum Flash frequency in MHz. Most Flash chips can run fine up to 126 MHz, but sometimes it needs to be tweaked for improving stability.

* **PSRAM** - same as FLASH but for PSRAM.

* **FLASH_T/PSRAM_T** - Flash/PSRAM timing values in form of QMI::M0/M1_TIMING register values.

* **AUDIO** - select audio output (I2S or PWM). If omitted, it's autodetected.

* **VIDEO** - select video output (VGA, DVI or HDMI). If omitted, it's autodetected. DVI and HDMI are effectively the same setting. 

* **VOLUME** - audio output volume after SFX/CD Audio mixing (0 - quiet, 100 - maximum)

* **HSTX_PINMAP** (HSTX builds only) - override HSTX pin mapping. Expressed as 32-bit hexadecimal number made out of 8 nibbles in this order (left to right):
  `0x[Lane2+][Lane2-][Lane1+][Lane1-][Lane0+][Lane0-][Clock+][Clock-]`

  Each nibble contains an offset of corresponding DVI/HDMI signal from HSTX GPIO base (12).
  Examples:

  * `HSTX_PINMAP=76543210` - Murmulator 2
  * `HSTX_PINMAP=76540123` - Pico-DVI-Sock

## known issues

* a bit of cracking in sound - will be addressed later
* PS/2 mouse driver has high CPU overhead - prefer USB HID over PS/2

## credits

original repo: https://github.com/erysdren/quakegeneric (for MS-DOS / SDL2 / Win32)

RP2350 port maintained by @Michael_V1973 (https://github.com/DnCraptor)

Additional tweaks and performance optimizations by Artem Vasilev aka (https://github.com/wbcbz7)

[INSERT MORE CREDITS ;)]

Big thanks to [Murmulator Telegram dev group](https://t.me/ZX_MURMULATOR) for drivers, useful tips, suggestions and beta-testing! ;)

## License

```
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
```


Some additional code like VGA/HDMI drivers are licensed under different terms - see the `drivers` directory.