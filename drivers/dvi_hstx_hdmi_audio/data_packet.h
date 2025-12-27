#pragma once
#include <stdint.h>
#include "pico.h"

#define TMDS_CHANNELS        3
#define N_LINE_PER_DATA      2
#define W_GUARDBAND          2
#define W_PREAMBLE           8
#define W_DATA_PACKET        32

#define W_DATA_ISLAND             (W_GUARDBAND * 2 + W_DATA_PACKET)
#define N_DATA_ISLAND_WORDS_HSTX  (W_DATA_ISLAND)

#define DATA_ISLAND_H0  (0 << 0)
#define DATA_ISLAND_V0  (0 << 1)
#define DATA_ISLAND_H1  (1 << 0)
#define DATA_ISLAND_V1  (1 << 1)

// AVI InfoFrame flags
enum {
    // byte 1
    AVI_PIXEL_FORMAT_RGB      = (0 << 5),
    AVI_PIXEL_FORMAT_YCBCR422 = (1 << 5),
    AVI_PIXEL_FORMAT_YCBCR444 = (2 << 5),

    AVI_ACTIVE_FORMAT_VALID   = (1 << 4),

    AVI_SCAN_INFO_NO_DATA     = (0 << 0),
    AVI_SCAN_INFO_OVERSCAN    = (1 << 0),
    AVI_SCAN_INFO_UNDERSCAN   = (2 << 0),

    // byte 2
    AVI_COLORIMETRY_NO_DATA   = (0 << 6),
    AVI_COLORIMETRY_BT601     = (1 << 6),
    AVI_COLORIMETRY_BT709     = (2 << 6),
    AVI_COLORIMETRY_EXTENDED  = (3 << 6),

    AVI_ASPECT_RATIO_NO_DATA  = (0 << 4),
    AVI_ASPECT_RATIO_4_3      = (1 << 4),
    AVI_ASPECT_RATIO_16_9     = (2 << 4),

    AVI_ACTIVE_FORMAT_SAME    = (8  << 0),
    AVI_ACTIVE_FORMAT_4_3     = (9  << 0),
    AVI_ACTIVE_FORMAT_16_9    = (10 << 0),
    AVI_ACTIVE_FORMAT_14_9    = (11 << 0),

    // byte 3
    AVI_IT_CONTENT            = (1 << 7),

    AVI_COLOR_RANGE_DEFAULT   = (0 << 2),
    AVI_COLOR_RANGE_LIMITED   = (1 << 2),
    AVI_COLOR_RANGE_FULL      = (2 << 2),
};

// video information code (AVI infoframe byte 4)
enum {
    VIC_NOT_SPECIFIED       = 0,
    VIC_640x480p_60hz       = 1,
    VIC_720x480p_60hz       = 2,
    VIC_720x480p_60hz_16_9  = 3,
    VIC_1280x720p_60hz      = 4,
    VIC_1920x1080i_60hz     = 5,
    VIC_1440x480i_60hz      = 6,
    VIC_1440x480i_60hz_16_9 = 7,
    VIC_1440x240p_60hz      = 8,
    VIC_1440x240p_60hz_16_9 = 9,
    VIC_1920x1080p_60hz     = 16,
    VIC_720x576p_50hz       = 17,
    VIC_720x576p_50hz_16_9  = 18,
    VIC_1280x720p_50hz      = 19,
    VIC_1920x1080i_50hz     = 20,
    VIC_1440x576i_50hz      = 21,
    VIC_1440x576i_50hz_16_9 = 22,
    VIC_1440x288p_50hz      = 23,
    VIC_1440x288p_50hz_16_9 = 24,
    VIC_1920x1080p_50hz     = 31,
};

typedef struct data_packet {
    uint8_t header[4];
    union {
        uint8_t subpacket[4][8];
        uint8_t packet[32];
    };
} data_packet_t;

typedef uint32_t data_island_stream_hstx_t[N_DATA_ISLAND_WORDS_HSTX];

// Functions related to the data_packet (requires a data_packet instance)
void compute_header_parity(data_packet_t *data_packet);
void compute_subpacket_parity(data_packet_t *data_packet, int i);
void compute_parity(data_packet_t *data_packet);
void compute_info_frame_checkSum(data_packet_t *data_packet);
void encode_header(const data_packet_t *data_packet, uint32_t *dst, int hv, bool firstPacket);
void encode_subpacket(const data_packet_t *data_packet, uint32_t *dst1, uint32_t *dst2);
void set_null(data_packet_t *data_packet);
void set_AVI_info_frame(data_packet_t *data_packet, uint8_t byte1, uint8_t byte2, uint8_t byte3, int vic);
void set_audio_info_frame(data_packet_t *data_packet, int freq);
void set_audio_clock_regeneration(data_packet_t *data_packet, int cts, int n);
int  set_audio_sample(data_packet_t *data_packet, const int16_t *p, int n, int frameCt);
void set_general_control_packet(data_packet_t *data_packet, bool avmute);

void hdmi_audio_set_audio_sample_header(data_packet_t *data_packet, int first);
int hdmi_audio_set_audio_sample_data(data_packet_t *data_packet, const int16_t *p, int frameCt);
int hdmi_audio_set_audio_sample(data_packet_t *data_packet, const int16_t *p, int samples, int frameCt);
uint32_t encode_data_islands_hstx(uint32_t *dst, const data_packet_t *packet, size_t num_packets, int polarity);
uint32_t encode_data_islands_header_only_hstx(uint32_t *dst, const data_packet_t *packet, size_t num_packets, int polarity);
