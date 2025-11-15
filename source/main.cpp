#pragma GCC optimize("Ofast")

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <time.h>

#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pio.h>
#include <hardware/i2c.h>
#include <hardware/vreg.h>
#include <hardware/sync.h>
#include <hardware/flash.h>
#include <hardware/clocks.h>
#include <hardware/structs/systick.h>

#ifdef PICO_RP2350
#include <hardware/regs/qmi.h>
#include <hardware/structs/qmi.h>
#endif

#include "graphics.h"

#include "audio.h"
#include "ff.h"
#include "psram_spi.h"
#ifdef KBDUSB
    #include "ps2kbd_mrmltr.h"
#else
    #include "ps2.h"
#endif

#if USE_NESPAD
#include "nespad.h"
#endif

#include "quakegeneric.h"

#define HOME_DIR (char*)"\\QUAKE"

bool rp2350a = true;
uint8_t rx[4] = { 0 };

struct semaphore vga_start_semaphore;

static FATFS fs;

struct input_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};

input_bits_t gamepad1_bits = { false, false, false, false, false, false, false, false };
static input_bits_t gamepad2_bits = { false, false, false, false, false, false, false, false };

extern "C" bool handleScancode(const uint32_t ps2scancode) {
    #if 0
    if (ps2scancode != 0x45 && ps2scancode != 0x1D && ps2scancode != 0xC5) {
        char tmp1[16];
        snprintf(tmp1, 16, "%08X", ps2scancode);
        OSD::osdCenteredMsg(tmp1, LEVEL_WARN, 500);
    }
    #endif
    static bool pause_detected = false;
    if (pause_detected) {
        pause_detected = false;
        if (ps2scancode == 0x1D) return true; // ignore next byte after 0x45, TODO: split with NumLock
    }
    if ( ((ps2scancode >> 8) & 0xFF) == 0xE0) { // E0 block
        uint8_t cd = ps2scancode & 0xFF;
        bool pressed = cd < 0x80;
        cd &= 0x7F;
        #if 0
        switch (cd) {
            case 0x5B: kbdPushData(fabgl::VirtualKey::VK_LCTRL, pressed); return true; /// L WIN
            case 0x1D: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_ALTFIRE, pressed);
                kbdPushData(fabgl::VirtualKey::VK_RCTRL, pressed);
                return true;
            }
            case 0x38: kbdPushData(fabgl::VirtualKey::VK_RALT, pressed); return true;
            case 0x5C: {  /// R WIN
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_ALTFIRE, pressed);
                kbdPushData(fabgl::VirtualKey::VK_RCTRL, pressed);
                return true;
            }
            case 0x5D: kbdPushData(fabgl::VirtualKey::VK_F1, pressed); return true; /// MENU
            case 0x37: kbdPushData(fabgl::VirtualKey::VK_PRINTSCREEN, pressed); return true;
            case 0x46: kbdPushData(fabgl::VirtualKey::VK_BREAK, pressed); return true;
            case 0x52: kbdPushData(fabgl::VirtualKey::VK_INSERT, pressed); return true;
            case 0x47: {
                joyPushData(fabgl::VirtualKey::VK_MENU_HOME, pressed);
                kbdPushData(fabgl::VirtualKey::VK_HOME, pressed);
                return true;
            }
            case 0x4F: kbdPushData(fabgl::VirtualKey::VK_END, pressed); return true;
            case 0x49: kbdPushData(fabgl::VirtualKey::VK_PAGEUP, pressed); return true;
            case 0x51: kbdPushData(fabgl::VirtualKey::VK_PAGEDOWN, pressed); return true;
            case 0x53: kbdPushData(fabgl::VirtualKey::VK_DELETE, pressed); return true;
            case 0x48: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_UP, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_UP, pressed);
                kbdPushData(fabgl::VirtualKey::VK_UP, pressed);
                return true;
            }
            case 0x50: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_DOWN, pressed);
                kbdPushData(fabgl::VirtualKey::VK_DOWN, pressed);
                return true;
            }
            case 0x4B: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_LEFT, pressed);
                kbdPushData(fabgl::VirtualKey::VK_LEFT, pressed);
                return true;
            }
            case 0x4D: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_RIGHT, pressed);
                kbdPushData(fabgl::VirtualKey::VK_RIGHT, pressed);
                return true;
            }
            case 0x35: kbdPushData(fabgl::VirtualKey::VK_SLASH, pressed); return true;
            case 0x1C: { // VK_KP_ENTER
                kbdPushData(Config::rightSpace ? fabgl::VirtualKey::VK_SPACE : fabgl::VirtualKey::VK_RETURN, pressed);
                return true;
            }
        }
        #endif
        return true;
    }
    uint8_t cd = ps2scancode & 0xFF;
    bool pressed = cd < 0x80;
    cd &= 0x7F;
    #if 0
    switch (cd) {
        case 0x1E: kbdPushData(fabgl::VirtualKey::VK_A, pressed); return true;
        case 0x30: kbdPushData(fabgl::VirtualKey::VK_B, pressed); return true;
        case 0x2E: kbdPushData(fabgl::VirtualKey::VK_C, pressed); return true;
        case 0x20: kbdPushData(fabgl::VirtualKey::VK_D, pressed); return true;
        case 0x12: kbdPushData(fabgl::VirtualKey::VK_E, pressed); return true;
        case 0x21: kbdPushData(fabgl::VirtualKey::VK_F, pressed); return true;
        case 0x22: kbdPushData(fabgl::VirtualKey::VK_G, pressed); return true;
        case 0x23: kbdPushData(fabgl::VirtualKey::VK_H, pressed); return true;
        case 0x17: kbdPushData(fabgl::VirtualKey::VK_I, pressed); return true;
        case 0x24: kbdPushData(fabgl::VirtualKey::VK_J, pressed); return true;
        case 0x25: kbdPushData(fabgl::VirtualKey::VK_K, pressed); return true;
        case 0x26: kbdPushData(fabgl::VirtualKey::VK_L, pressed); return true;
        case 0x32: kbdPushData(fabgl::VirtualKey::VK_M, pressed); return true;
        case 0x31: kbdPushData(fabgl::VirtualKey::VK_N, pressed); return true;
        case 0x18: kbdPushData(fabgl::VirtualKey::VK_O, pressed); return true;
        case 0x19: kbdPushData(fabgl::VirtualKey::VK_P, pressed); return true;
        case 0x10: kbdPushData(fabgl::VirtualKey::VK_Q, pressed); return true;
        case 0x13: kbdPushData(fabgl::VirtualKey::VK_R, pressed); return true;
        case 0x1F: kbdPushData(fabgl::VirtualKey::VK_S, pressed); return true;
        case 0x14: kbdPushData(fabgl::VirtualKey::VK_T, pressed); return true;
        case 0x16: kbdPushData(fabgl::VirtualKey::VK_U, pressed); return true;
        case 0x2F: kbdPushData(fabgl::VirtualKey::VK_V, pressed); return true;
        case 0x11: kbdPushData(fabgl::VirtualKey::VK_W, pressed); return true;
        case 0x2D: kbdPushData(fabgl::VirtualKey::VK_X, pressed); return true;
        case 0x15: kbdPushData(fabgl::VirtualKey::VK_Y, pressed); return true;
        case 0x2C: kbdPushData(fabgl::VirtualKey::VK_Z, pressed); return true;

        case 0x0B: kbdPushData(fabgl::VirtualKey::VK_0, pressed); return true;
        case 0x02: kbdPushData(fabgl::VirtualKey::VK_1, pressed); return true;
        case 0x03: kbdPushData(fabgl::VirtualKey::VK_2, pressed); return true;
        case 0x04: kbdPushData(fabgl::VirtualKey::VK_3, pressed); return true;
        case 0x05: kbdPushData(fabgl::VirtualKey::VK_4, pressed); return true;
        case 0x06: kbdPushData(fabgl::VirtualKey::VK_5, pressed); return true;
        case 0x07: kbdPushData(fabgl::VirtualKey::VK_6, pressed); return true;
        case 0x08: kbdPushData(fabgl::VirtualKey::VK_7, pressed); return true;
        case 0x09: kbdPushData(fabgl::VirtualKey::VK_8, pressed); return true;
        case 0x0A: kbdPushData(fabgl::VirtualKey::VK_9, pressed); return true;

        case 0x29: kbdPushData(fabgl::VirtualKey::VK_TILDE, pressed); return true;
        case 0x0C: kbdPushData(fabgl::VirtualKey::VK_MINUS, pressed); return true;
        case 0x0D: kbdPushData(fabgl::VirtualKey::VK_EQUALS, pressed); return true;
        case 0x2B: kbdPushData(fabgl::VirtualKey::VK_BACKSLASH, pressed); return true;
        case 0x1A: kbdPushData(fabgl::VirtualKey::VK_LEFTBRACKET, pressed); return true;
        case 0x1B: kbdPushData(fabgl::VirtualKey::VK_RIGHTBRACKET, pressed); return true;
        case 0x27: kbdPushData(fabgl::VirtualKey::VK_SEMICOLON, pressed); return true;
        case 0x28: kbdPushData(fabgl::VirtualKey::VK_QUOTE, pressed); return true;
        case 0x33: kbdPushData(fabgl::VirtualKey::VK_COMMA, pressed); return true;
        case 0x34: kbdPushData(fabgl::VirtualKey::VK_PERIOD, pressed); return true;
        case 0x35: kbdPushData(fabgl::VirtualKey::VK_SLASH, pressed); return true;

        case 0x0E: {
            joyPushData(fabgl::VirtualKey::VK_MENU_BS, pressed);
            kbdPushData(fabgl::VirtualKey::VK_BACKSPACE, pressed);
            return true;
        }
        case 0x39: {
            joyPushData(fabgl::VirtualKey::VK_MENU_ENTER, pressed);
            kbdPushData(fabgl::VirtualKey::VK_SPACE, pressed);
            return true;
        }
        case 0x0F: {
            if (Config::TABasfire1) JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_TAB, pressed);
            return true;
        }
        case 0x3A: kbdPushData(fabgl::VirtualKey::VK_CAPSLOCK, pressed); return true; /// TODO: CapsLock
        case 0x2A: kbdPushData(fabgl::VirtualKey::VK_LSHIFT, pressed); return true;
        case 0x1D: kbdPushData(fabgl::VirtualKey::VK_LCTRL, pressed); return true;
        case 0x38: {
            if (Config::CursorAsJoy) JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_LALT, pressed);
            return true;
        }
        case 0x36: kbdPushData(fabgl::VirtualKey::VK_RSHIFT, pressed); return true;
        case 0x1C: {
            joyPushData(fabgl::VirtualKey::VK_MENU_ENTER, pressed);
            kbdPushData(fabgl::VirtualKey::VK_RETURN, pressed);
            return true;
        }
        case 0x01: kbdPushData(fabgl::VirtualKey::VK_ESCAPE, pressed); return true;
        case 0x3B: kbdPushData(fabgl::VirtualKey::VK_F1, pressed); return true;
        case 0x3C: kbdPushData(fabgl::VirtualKey::VK_F2, pressed); return true;
        case 0x3D: kbdPushData(fabgl::VirtualKey::VK_F3, pressed); return true;
        case 0x3E: kbdPushData(fabgl::VirtualKey::VK_F4, pressed); return true;
        case 0x3F: kbdPushData(fabgl::VirtualKey::VK_F5, pressed); return true;
        case 0x40: kbdPushData(fabgl::VirtualKey::VK_F6, pressed); return true;
        case 0x41: kbdPushData(fabgl::VirtualKey::VK_F7, pressed); return true;
        case 0x42: kbdPushData(fabgl::VirtualKey::VK_F8, pressed); return true;
        case 0x43: kbdPushData(fabgl::VirtualKey::VK_F9, pressed); return true;
        case 0x44: kbdPushData(fabgl::VirtualKey::VK_F10, pressed); return true;
        case 0x57: kbdPushData(fabgl::VirtualKey::VK_F11, pressed); return true;
        case 0x58: kbdPushData(fabgl::VirtualKey::VK_F12, pressed); return true;

        case 0x46: kbdPushData(fabgl::VirtualKey::VK_SCROLLLOCK, pressed); return true; /// TODO:
        case 0x45: {
            kbdPushData(fabgl::VirtualKey::VK_PAUSE, pressed);
            pause_detected = pressed;
            return true;
        }
        case 0x37: {
            JPAD(fabgl::VirtualKey::VK_DPAD_START, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_MULTIPLY, pressed);
            return true;
        }
        case 0x4A: {
            JPAD(fabgl::VirtualKey::VK_DPAD_SELECT, pressed);
            kbdPushData(fabgl::VirtualKey::VK_MINUS, pressed);
            return true;
        }
        case 0x4E: {
            JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_PLUS, pressed);
            return true;
        }
        case 0x53: {
            JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_PERIOD, pressed);
            return true;
        }
        case 0x52: {
            JPAD(fabgl::VirtualKey::VK_DPAD_ALTFIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_0, pressed);
            return true;
        }
        case 0x4F: {
            JPAD(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_1, pressed);
            return true;
        }
        case 0x50: {
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_2, pressed);
            return true;
        }
        case 0x51: {
            JPAD(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_3, pressed);
            return true;
        }
        case 0x4B: {
            JPAD(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_4, pressed);
            return true;
        }
        case 0x4C: {
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_5, pressed);
            return true;
        }
        case 0x4D: {
            JPAD(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_6, pressed);
            return true;
        }
        case 0x47: {
            JPAD(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_UP, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_7, pressed);
            return true;
        }
        case 0x48: {
            JPAD(fabgl::VirtualKey::VK_DPAD_UP, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_8, pressed);
            return true;
        }
        case 0x49: {
            JPAD(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_UP, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_9, pressed);
            return true;
        }
    }
    #endif
    return true;
}


#if USE_NESPAD

static void nespad_tick(void) {
    nespad_read();
    gamepad1_bits.a = (nespad_state & DPAD_A) != 0;
    gamepad1_bits.b = (nespad_state & DPAD_B) != 0;
    gamepad1_bits.start = (nespad_state & DPAD_START) != 0;
    gamepad1_bits.up = (nespad_state & DPAD_UP) != 0;
    gamepad1_bits.down = (nespad_state & DPAD_DOWN) != 0;
    gamepad1_bits.left = (nespad_state & DPAD_LEFT) != 0;
    gamepad1_bits.right = (nespad_state & DPAD_RIGHT) != 0;

    gamepad2_bits.a = (nespad_state2 & DPAD_A) != 0;
    gamepad2_bits.b = (nespad_state2 & DPAD_B) != 0;
    gamepad2_bits.select = (nespad_state2 & DPAD_SELECT) != 0;
    gamepad2_bits.start = (nespad_state2 & DPAD_START) != 0;
    gamepad2_bits.up = (nespad_state2 & DPAD_UP) != 0;
    gamepad2_bits.down = (nespad_state2 & DPAD_DOWN) != 0;
    gamepad2_bits.left = (nespad_state2 & DPAD_LEFT) != 0;
    gamepad2_bits.right = (nespad_state2 & DPAD_RIGHT) != 0;
}

#endif

#ifdef KBDUSB
inline static bool isInReport(hid_keyboard_report_t const *report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

void __not_in_flash_func(process_kbd_report)(
    hid_keyboard_report_t const *report,
    hid_keyboard_report_t const *prev_report
) {

}

Ps2Kbd_Mrmltr ps2kbd(
        pio1,
        KBD_CLOCK_PIN,
        process_kbd_report
);
#endif

void repeat_me_for_input() {
    static uint32_t tickKbdRep1 = time_us_32();
    // 60 FPS loop
#define frame_tick (16666)
    static uint64_t tick = time_us_64();
    static uint64_t last_input_tick = tick;
        if (tick >= last_input_tick + frame_tick) {
#ifdef KBDUSB
            ps2kbd.tick();
#endif
#ifdef USE_NESPAD
            nespad_tick();
#endif
            last_input_tick = tick;
        }
        tick = time_us_64();
#ifdef KBDUSB
        tuh_task();
#endif
}

uint8_t FRAME_BUF[QUAKEGENERIC_RES_X * QUAKEGENERIC_RES_Y] = { 0 };

void __scratch_x("render") render_core() {
    multicore_lockout_victim_init();
    graphics_init();
    graphics_set_buffer(FRAME_BUF, QUAKEGENERIC_RES_X, QUAKEGENERIC_RES_Y);
    graphics_set_bgcolor(0x000000);
    graphics_set_flashmode(false, false);
    sem_acquire_blocking(&vga_start_semaphore);
    while (true) {
        tight_loop_contents();
    }
    __unreachable();
}

#if SOFTTV
typedef struct tv_out_mode_t {
    // double color_freq;
    float color_index;
    COLOR_FREQ_t c_freq;
    enum graphics_mode_t mode_bpp;
    g_out_TV_t tv_system;
    NUM_TV_LINES_t N_lines;
    bool cb_sync_PI_shift_lines;
    bool cb_sync_PI_shift_half_frame;
} tv_out_mode_t;
extern tv_out_mode_t tv_out_mode;

bool color_mode=true;
bool toggle_color() {
    color_mode=!color_mode;
    if(color_mode) {
        tv_out_mode.color_index= 1.0f;
    } else {
        tv_out_mode.color_index= 0.0f;
    }

    return true;
}
#endif

uint8_t psram_pin;
#if PICO_RP2350
#include <hardware/exception.h>
#include <hardware/structs/qmi.h>
#include <hardware/structs/xip.h>
#include <hardware/regs/sysinfo.h>

#ifdef BUTTER_PSRAM_GPIO
#define MB16 (16ul << 20)
#define MB8 (8ul << 20)
#define MB4 (4ul << 20)
#define MB1 (1ul << 20)
uint8_t* PSRAM_DATA = (uint8_t*)0x11000000;
static int BUTTER_PSRAM_SIZE = -1;
uint32_t __not_in_flash_func(butter_psram_size)() {
    if (BUTTER_PSRAM_SIZE != -1) return BUTTER_PSRAM_SIZE;
    for(register int i = MB8; i < MB16; i += 4096)
        PSRAM_DATA[i] = 16;
    for(register int i = MB4; i < MB8; i += 4096)
        PSRAM_DATA[i] = 8;
    for(register int i = MB1; i < MB4; i += 4096)
        PSRAM_DATA[i] = 4;
    for(register int i = 0; i < MB1; i += 4096)
        PSRAM_DATA[i] = 1;
    register uint32_t res = PSRAM_DATA[MB16 - 4096];
    for (register int i = MB16 - MB1; i < MB16; i += 4096) {
        if (res != PSRAM_DATA[i])
            return 0;
    }
    BUTTER_PSRAM_SIZE = res << 20;
    return BUTTER_PSRAM_SIZE;
}
void __no_inline_not_in_flash_func(psram_init)(uint cs_pin) {
    gpio_set_function(cs_pin, GPIO_FUNC_XIP_CS1);

    // Enable direct mode, PSRAM CS, clkdiv of 10.
    qmi_hw->direct_csr = 10 << QMI_DIRECT_CSR_CLKDIV_LSB | \
                               QMI_DIRECT_CSR_EN_BITS | \
                               QMI_DIRECT_CSR_AUTO_CS1N_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;

    // Enable QPI mode on the PSRAM
    const uint CMD_QPI_EN = 0x35;
    qmi_hw->direct_tx = QMI_DIRECT_TX_NOPUSH_BITS | CMD_QPI_EN;

    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;

    // Set PSRAM timing for APS6404
    //
    // Using an rxdelay equal to the divisor isn't enough when running the APS6404 close to 133MHz.
    // So: don't allow running at divisor 1 above 100MHz (because delay of 2 would be too late),
    // and add an extra 1 to the rxdelay if the divided clock is > 100MHz (i.e. sys clock > 200MHz).
    const int max_psram_freq = 166000000;
    const int clock_hz = clock_get_hz(clk_sys);
    int divisor = (clock_hz + max_psram_freq - 1) / max_psram_freq;
    if (divisor == 1 && clock_hz > 100000000) {
        divisor = 2;
    }
    int rxdelay = divisor;
    if (clock_hz / divisor > 100000000) {
        rxdelay += 1;
    }

    // - Max select must be <= 8us.  The value is given in multiples of 64 system clocks.
    // - Min deselect must be >= 18ns.  The value is given in system clock cycles - ceil(divisor / 2).
    const int clock_period_fs = 1000000000000000ll / clock_hz;
    const int max_select = (125 * 1000000) / clock_period_fs;  // 125 = 8000ns / 64
    const int min_deselect = (18 * 1000000 + (clock_period_fs - 1)) / clock_period_fs - (divisor + 1) / 2;

    qmi_hw->m[1].timing = 1 << QMI_M1_TIMING_COOLDOWN_LSB |
                          QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
                          max_select << QMI_M1_TIMING_MAX_SELECT_LSB |
                          min_deselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
                          rxdelay << QMI_M1_TIMING_RXDELAY_LSB |
                          divisor << QMI_M1_TIMING_CLKDIV_LSB;

    // Set PSRAM commands and formats
    qmi_hw->m[1].rfmt =
        QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB |\
        QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q   << QMI_M0_RFMT_ADDR_WIDTH_LSB |\
        QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB |\
        QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q  << QMI_M0_RFMT_DUMMY_WIDTH_LSB |\
        QMI_M0_RFMT_DATA_WIDTH_VALUE_Q   << QMI_M0_RFMT_DATA_WIDTH_LSB |\
        QMI_M0_RFMT_PREFIX_LEN_VALUE_8   << QMI_M0_RFMT_PREFIX_LEN_LSB |\
        6                                << QMI_M0_RFMT_DUMMY_LEN_LSB;

    qmi_hw->m[1].rcmd = 0xEB;

    qmi_hw->m[1].wfmt =
        QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB |\
        QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q   << QMI_M0_WFMT_ADDR_WIDTH_LSB |\
        QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB |\
        QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q  << QMI_M0_WFMT_DUMMY_WIDTH_LSB |\
        QMI_M0_WFMT_DATA_WIDTH_VALUE_Q   << QMI_M0_WFMT_DATA_WIDTH_LSB |\
        QMI_M0_WFMT_PREFIX_LEN_VALUE_8   << QMI_M0_WFMT_PREFIX_LEN_LSB;

    qmi_hw->m[1].wcmd = 0x38;

    // Disable direct mode
    qmi_hw->direct_csr = 0;

    // Enable writes to PSRAM
    hw_set_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_WRITABLE_M1_BITS);

    // init size
    butter_psram_size();
}
#else
uint8_t* PSRAM_DATA = (uint8_t*)0;
uint32_t __not_in_flash_func(butter_psram_size)() { return 0; }
#endif
#else
uint8_t* PSRAM_DATA = (uint8_t*)0;
uint32_t __not_in_flash_func(butter_psram_size)() { return 0; }
#endif

void sigbus(void) {
    printf("SIGBUS exception caught...\n");
    // reset_usb_boot(0, 0);
}
void __attribute__((naked, noreturn)) __printflike(1, 0) dummy_panic(__unused const char *fmt, ...) {
    printf("*** PANIC ***\n");
    if (fmt)
        printf(fmt);
}

#ifndef PICO_RP2040
void __not_in_flash() flash_timings() {
        const int max_flash_freq = 88 * MHZ;
        const int clock_hz = CPU_MHZ * MHZ;
        int divisor = (clock_hz + max_flash_freq - 1) / max_flash_freq;
        if (divisor == 1 && clock_hz > 100000000) {
            divisor = 2;
        }
        int rxdelay = divisor;
        if (clock_hz / divisor > 100000000) {
            rxdelay += 1;
        }
        qmi_hw->m[0].timing = 0x60007000 |
                            rxdelay << QMI_M0_TIMING_RXDELAY_LSB |
                            divisor << QMI_M0_TIMING_CLKDIV_LSB;
}
#endif

static void __not_in_flash_func(flash_info)() {
    if (rx[0] == 0) {
        uint8_t tx[4] = {0x9f};
        flash_do_cmd(tx, rx, 4);
    }
}

extern "C" void vsync_handler() {

}

extern "C" uint8_t* get_line_buffer(int line) {
    return FRAME_BUF + QUAKEGENERIC_RES_X * line;
}

extern "C" int get_video_mode() {
    return 0;
}

extern "C" void QG_Init(void) {

}

extern "C" int QG_GetKey(int *down, int *key) {
	return 0;
}

extern "C" void QG_GetMouseMove(int *x, int *y) {

}

extern "C" void QG_Quit(void) {

}

extern "C" void QG_DrawFrame(void *pixels) {
	memcpy(FRAME_BUF, pixels, QUAKEGENERIC_RES_X * QUAKEGENERIC_RES_Y);
}

extern "C" void QG_SetPalette(unsigned char palette[768]) {

}

extern "C" void QG_GetJoyAxes(float *axes)
{
    *axes = 0;
}


int main() {
    flash_info();
#ifdef PICO_RP2040
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    set_sys_clock_khz(CPU_MHZ * KHZ, true);
    // #define QMI_COOLDOWN 30     // 0xc0000000 [31:30] COOLDOWN     (0x1) Chip select cooldown period
    // #define QMI_PAGEBREAK 28    // 0x30000000 [29:28] PAGEBREAK    (0x0) When page break is enabled, chip select will...
    // #define QMI_SELECT_SETUP 25 // 0x02000000 [25]    SELECT_SETUP (0) Add up to one additional system clock cycle of setup...
    // #define QMI_SELECT_HOLD 23  // 0x01800000 [24:23] SELECT_HOLD  (0x0) Add up to three additional system clock cycles of active...
    // #define QMI_MAX_SELECT 17   // 0x007e0000 [22:17] MAX_SELECT   (0x00) Enforce a maximum assertion duration for this window's...
    // #define QMI_MIN_DESELECT 12 // 0x0001f000 [16:12] MIN_DESELECT (0x00) After this window's chip select is deasserted, it...
    // #define QMI_RXDELAY 8       // 0x00000700 [10:8]  RXDELAY      (0x0) Delay the read data sample timing, in units of one half...
    // #define QMI_CLKDIV 0        // 0x000000ff [7:0]   CLKDIV       (0x04) Clock divisor
    // qmi_hw->m[1].timing = (1 << QMI_COOLDOWN) | (2 << QMI_PAGEBREAK) | (3 << QMI_SELECT_HOLD) | (18 << QMI_MAX_SELECT) | (4 << QMI_MIN_DESELECT) | (6 << QMI_RXDELAY) | (6 << QMI_CLKDIV);

    // uint clkdiv = 4;
    // uint rxdelay = 4;
    // hw_write_masked(
    // &qmi_hw->m[0].timing,
    //         ((clkdiv << QMI_M0_TIMING_CLKDIV_LSB) & QMI_M0_TIMING_CLKDIV_BITS) |
    //         ((rxdelay << QMI_M0_TIMING_RXDELAY_LSB) & QMI_M0_TIMING_RXDELAY_BITS),
    //         QMI_M0_TIMING_CLKDIV_BITS | QMI_M0_TIMING_RXDELAY_BITS
    // );
    // busy_wait_us(1000);
    // set_sys_clock_khz(CPU_MHZ * KHZ, true);

#else
    #if 0
        vreg_set_voltage(VREG_VOLTAGE_1_10); // Set voltage  //
        delay(100);
        set_sys_clock_khz(CPU_MHZ * KHZ, true);
    #else
        vreg_disable_voltage_limit();
        vreg_set_voltage(VREG_VOLTAGE_1_60);
        flash_timings();
        sleep_ms(100);

        if (!set_sys_clock_khz(CPU_MHZ * KHZ, 0)) {
            #undef CPU_MHZ
            #define CPU_MHZ 252
            set_sys_clock_khz(CPU_MHZ * KHZ, 1); // fallback to failsafe clocks
        }
    #endif
#endif

#ifdef KBDUSB
    tuh_init(BOARD_TUH_RHPORT);
    ps2kbd.init_gpio();
#else
    keyboard_init();
#endif

    #ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
    #endif

#if PICO_RP2350
    rp2350a = (*((io_ro_32*)(SYSINFO_BASE + SYSINFO_PACKAGE_SEL_OFFSET)) & 1);
    #ifdef BUTTER_PSRAM_GPIO
        psram_pin = rp2350a ? BUTTER_PSRAM_GPIO : 47;
        psram_init(psram_pin);
        butter_psram_size();
    #endif
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, sigbus);
#endif


#if USE_NESPAD
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
#endif

#if PICO_RP2350
    if (psram_pin != PSRAM_PIN_SCK)
#endif
    #ifndef MURM2
        init_psram();
    #endif
    // send kbd reset only after initial process passed
#ifndef KBDUSB
    keyboard_send(0xFF);
#endif

    int argc = 1;
    char* argv[] = {
        "quake",
        0
    };
	QG_Create(argc, argv);

    #ifdef PICO_DEFAULT_LED_PIN
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }
    #endif
    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);

    const double ticks_per_second = clock_get_hz(clk_sys);
    // Настраиваем SysTick: тактирование от системной частоты
    // и максимальное значение 24-битного счётчика
    systick_hw->rvr = 0xFFFFFF;  // reload value (24 бита макс)
    systick_hw->cvr = 0;         // current value
    systick_hw->csr = (1 << 0) | (1 << 2);  // ENABLE = бит 0, CLKSOURCE = бит 2
    uint32_t start = systick_hw->cvr;
	while (1) {
        // Считаем прошедшие такты ARM
        uint32_t now = systick_hw->cvr;
        // SysTick counts down, 24-bit wrap
        uint32_t elapsed_ticks = (start - now) & 0xFFFFFF;
		QG_Tick(elapsed_ticks / ticks_per_second);
        start = now;
	}

    __unreachable();
}
