/*
 * Implementation of HDMI data packet and info frame encoding
 * (removed the audio frame encoding not needed by hsdaoh)
 *
 * Copyright (c) 2021-2022 by Shuichi Takano
 * https://github.com/shuichitakano/pico_lib/blob/master/dvi/data_packet.cpp
 *
 * ported to C by Marcelo Lorenzati:
 * https://github.com/mlorenzati/PicoDVI/blob/master/software/libdvi/data_packet.c
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "data_packet.h"
#include <string.h>

// compute even parity
inline bool parity8(uint8_t index) {
    return 0x6996 >> ((index >> 4) ^ (index & 15)) & 1;
}
 
inline bool parity16(uint16_t i) {
    i ^= i >> 8;
    i ^= i >> 4;
    return (0x6996 >> (i & 15)) & 1;
}

// Compute 8 Parity End

// BCH Encoding Start
const uint8_t __not_in_flash("") bchTable_[256] = {
    0x00, 0xd9, 0xb5, 0x6c, 0x6d, 0xb4, 0xd8, 0x01, 
    0xda, 0x03, 0x6f, 0xb6, 0xb7, 0x6e, 0x02, 0xdb, 
    0xb3, 0x6a, 0x06, 0xdf, 0xde, 0x07, 0x6b, 0xb2, 
    0x69, 0xb0, 0xdc, 0x05, 0x04, 0xdd, 0xb1, 0x68, 
    0x61, 0xb8, 0xd4, 0x0d, 0x0c, 0xd5, 0xb9, 0x60, 
    0xbb, 0x62, 0x0e, 0xd7, 0xd6, 0x0f, 0x63, 0xba, 
    0xd2, 0x0b, 0x67, 0xbe, 0xbf, 0x66, 0x0a, 0xd3, 
    0x08, 0xd1, 0xbd, 0x64, 0x65, 0xbc, 0xd0, 0x09, 
    0xc2, 0x1b, 0x77, 0xae, 0xaf, 0x76, 0x1a, 0xc3, 
    0x18, 0xc1, 0xad, 0x74, 0x75, 0xac, 0xc0, 0x19, 
    0x71, 0xa8, 0xc4, 0x1d, 0x1c, 0xc5, 0xa9, 0x70, 
    0xab, 0x72, 0x1e, 0xc7, 0xc6, 0x1f, 0x73, 0xaa, 
    0xa3, 0x7a, 0x16, 0xcf, 0xce, 0x17, 0x7b, 0xa2, 
    0x79, 0xa0, 0xcc, 0x15, 0x14, 0xcd, 0xa1, 0x78, 
    0x10, 0xc9, 0xa5, 0x7c, 0x7d, 0xa4, 0xc8, 0x11, 
    0xca, 0x13, 0x7f, 0xa6, 0xa7, 0x7e, 0x12, 0xcb, 
    0x83, 0x5a, 0x36, 0xef, 0xee, 0x37, 0x5b, 0x82, 
    0x59, 0x80, 0xec, 0x35, 0x34, 0xed, 0x81, 0x58, 
    0x30, 0xe9, 0x85, 0x5c, 0x5d, 0x84, 0xe8, 0x31, 
    0xea, 0x33, 0x5f, 0x86, 0x87, 0x5e, 0x32, 0xeb, 
    0xe2, 0x3b, 0x57, 0x8e, 0x8f, 0x56, 0x3a, 0xe3, 
    0x38, 0xe1, 0x8d, 0x54, 0x55, 0x8c, 0xe0, 0x39, 
    0x51, 0x88, 0xe4, 0x3d, 0x3c, 0xe5, 0x89, 0x50, 
    0x8b, 0x52, 0x3e, 0xe7, 0xe6, 0x3f, 0x53, 0x8a, 
    0x41, 0x98, 0xf4, 0x2d, 0x2c, 0xf5, 0x99, 0x40, 
    0x9b, 0x42, 0x2e, 0xf7, 0xf6, 0x2f, 0x43, 0x9a, 
    0xf2, 0x2b, 0x47, 0x9e, 0x9f, 0x46, 0x2a, 0xf3, 
    0x28, 0xf1, 0x9d, 0x44, 0x45, 0x9c, 0xf0, 0x29, 
    0x20, 0xf9, 0x95, 0x4c, 0x4d, 0x94, 0xf8, 0x21, 
    0xfa, 0x23, 0x4f, 0x96, 0x97, 0x4e, 0x22, 0xfb, 
    0x93, 0x4a, 0x26, 0xff, 0xfe, 0x27, 0x4b, 0x92, 
    0x49, 0x90, 0xfc, 0x25, 0x24, 0xfd, 0x91, 0x48, 
};

__always_inline uint8_t encode_BCH_3(const uint8_t *p) {
    uint8_t v = bchTable_[p[0]];
    v = bchTable_[p[1] ^ v];
    v = bchTable_[p[2] ^ v];
    return v;
}

__always_inline uint8_t encode_BCH_7(const uint8_t *p) {
    uint8_t v = bchTable_[p[0]];
    v = bchTable_[p[1] ^ v];
    v = bchTable_[p[2] ^ v];
    v = bchTable_[p[3] ^ v];
    v = bchTable_[p[4] ^ v];
    v = bchTable_[p[5] ^ v];
    v = bchTable_[p[6] ^ v];
    return v;
}
// BCH Encoding End

// TERC4 Start
const uint16_t __not_in_flash("") TERC4Syms_[16] = {
    0b1010011100,
    0b1001100011,
    0b1011100100,
    0b1011100010,
    0b0101110001,
    0b0100011110,
    0b0110001110,
    0b0100111100,
    0b1011001100,
    0b0100111001,
    0b0110011100,
    0b1011000110,
    0b1010001110,
    0b1001110001,
    0b0101100011,
    0b1011000011,
};

// --------------
// data packet stuff

void compute_header_parity(data_packet_t *data_packet) {
    data_packet->header[3] = encode_BCH_3(data_packet->header);
}

void compute_subpacket_parity(data_packet_t *data_packet, int i) {
    data_packet->subpacket[i][7] = encode_BCH_7(data_packet->subpacket[i]);
}

void compute_parity(data_packet_t *data_packet) {
    compute_header_parity(data_packet);
    compute_subpacket_parity(data_packet, 0);
    compute_subpacket_parity(data_packet, 1);
    compute_subpacket_parity(data_packet, 2);
    compute_subpacket_parity(data_packet, 3);
}

void compute_info_frame_checkSum(data_packet_t *data_packet) {
    int s = 0;
    for (int i = 0; i < 3; ++i)
    {
        s += data_packet->header[i];
    }
    int n = data_packet->header[2] + 1;
    for (int j = 0; j < 4; ++j)
    {
        for (int i = 0; i < 7 && n; ++i, --n)
        {
            s += data_packet->subpacket[j][i];
        }
    }
    data_packet->subpacket[0][0] = -s;
}

void set_null(data_packet_t *data_packet) {
    memset(data_packet, 0, sizeof(data_packet_t));
}

int  __not_in_flash_func(hdmi_audio_set_audio_sample)(data_packet_t *data_packet, const int16_t *p, int samples, int frameCt) {
    int layout = 0;
    int samplePresent = (1 << samples)-1;
    int B = (frameCt < 4) ? 1 << (frameCt) : 0;
    data_packet->header[0] = 2;
    data_packet->header[1] = (layout << 4) | samplePresent;
    data_packet->header[2] = (B << 4);
    data_packet->header[3] = encode_BCH_3(data_packet->header);
    
    memset(data_packet->subpacket[0], 0, sizeof(data_packet->subpacket[0]));
    memset(data_packet->subpacket[1], 0, sizeof(data_packet->subpacket[1]));
    memset(data_packet->subpacket[2], 0, sizeof(data_packet->subpacket[2]));
    memset(data_packet->subpacket[3], 0, sizeof(data_packet->subpacket[3]));
    for (int i = 0; i < samples; ++i)
    {
        int l = p[0];
        int r = p[1];
        int vuc = 0; // valid
        uint8_t *d = data_packet->subpacket[i];
        *(int16_t*)&d[1] = l;
        *(int16_t*)&d[4] = r;
        int pl = parity16(l ^ vuc);
        int pr = parity16(l ^ vuc);
        d[6] = (vuc << 0) | (pl << 3) | (vuc << 4) | (pr << 7);
        d[7] = encode_BCH_7(d);
        p += 2;
    }

    frameCt += samples;
    if (frameCt >= 192) {
        frameCt -= 192;
    }
    return frameCt;
}

void set_general_control_packet(data_packet_t *data_packet, bool avmute) {
    data_packet->header[0] = 3;
    data_packet->header[1] = 0;
    data_packet->header[2] = 0;
    compute_header_parity(data_packet);

    data_packet->subpacket[0][0] = avmute ? 0x01 : 0x10;
    memset(&data_packet->subpacket[0][1], 0, 6*sizeof(uint8_t));
    compute_subpacket_parity(data_packet, 0);

    memcpy(data_packet->subpacket[1], data_packet->subpacket[0], sizeof(data_packet->subpacket[0]));
    memcpy(data_packet->subpacket[2], data_packet->subpacket[0], sizeof(data_packet->subpacket[0]));
    memcpy(data_packet->subpacket[3], data_packet->subpacket[0], sizeof(data_packet->subpacket[0]));
}

void set_AVI_info_frame(data_packet_t *data_packet, uint8_t byte1, uint8_t byte2, uint8_t byte3, int vic) {
    set_null(data_packet);
    data_packet->header[0] = 0x82;
    data_packet->header[1] = 2;  // version
    data_packet->header[2] = 13; // len

    data_packet->subpacket[0][1] = byte1;
    data_packet->subpacket[0][2] = byte2;
    data_packet->subpacket[0][3] = byte3;
    data_packet->subpacket[0][4] = vic;

    compute_info_frame_checkSum(data_packet);
    compute_parity(data_packet);
}

void set_audio_clock_regeneration(data_packet_t *data_packet, int cts, int n) {
    data_packet->header[0] = 1;
    data_packet->header[1] = 0;
    data_packet->header[2] = 0;
    compute_header_parity(data_packet);

    data_packet->subpacket[0][0] = 0;
    data_packet->subpacket[0][1] = cts >> 16;
    data_packet->subpacket[0][2] = cts >> 8;
    data_packet->subpacket[0][3] = cts;
    data_packet->subpacket[0][4] = n >> 16;
    data_packet->subpacket[0][5] = n >> 8;
    data_packet->subpacket[0][6] = n;
    compute_subpacket_parity(data_packet, 0);

    memcpy(data_packet->subpacket[1], data_packet->subpacket[0], sizeof(data_packet->subpacket[0]));
    memcpy(data_packet->subpacket[2], data_packet->subpacket[0], sizeof(data_packet->subpacket[0]));
    memcpy(data_packet->subpacket[3], data_packet->subpacket[0], sizeof(data_packet->subpacket[0]));
}

void set_audio_info_frame(data_packet_t *data_packet, int freq) {
    set_null(data_packet);
    data_packet->header[0] = 0x84;
    data_packet->header[1] = 1;  // version
    data_packet->header[2] = 10; // len

    const int cc = 1; // 2ch
    const int ct = 0; // "refer to stream header"
    const int ss = 0; // "refer to stream header"
    const int sf = freq == 48000 ? 3 : (freq == 44100 ? 2 : 0);
    const int ca = 0;  // FR, FL
    const int lsv = 0; // 0db
    const int dm_inh = 0;
    data_packet->subpacket[0][1] = cc | (ct << 4);
    data_packet->subpacket[0][2] = ss | (sf << 2);
    data_packet->subpacket[0][4] = ca;
    data_packet->subpacket[0][5] = (lsv << 3) | (dm_inh << 7);

    compute_info_frame_checkSum(data_packet);
    compute_parity(data_packet);
}

// -------------------
// data island encoding stuff

void __not_in_flash_func(encode_header_hstx)(const data_packet_t *data_packet, uint32_t *dst, int hv, bool firstPacket) {
    int hv1 = hv | 8;
    if (!firstPacket) { 
        hv = hv1;
    }
    #pragma GCC unroll 0
    for (int i = 0; i < 4; ++i) {
        uint32_t h = data_packet->header[i] << 2;
        dst[0] = TERC4Syms_[hv  | ((h >> 0) & 4)];  // first pixel at the start of data packet
        dst[1] = TERC4Syms_[hv1 | ((h >> 1) & 4)];
        dst[2] = TERC4Syms_[hv1 | ((h >> 2) & 4)];
        dst[3] = TERC4Syms_[hv1 | ((h >> 3) & 4)];
        dst[4] = TERC4Syms_[hv1 | ((h >> 4) & 4)];
        dst[5] = TERC4Syms_[hv1 | ((h >> 5) & 4)];
        dst[6] = TERC4Syms_[hv1 | ((h >> 6) & 4)];
        dst[7] = TERC4Syms_[hv1 | ((h >> 7) & 4)];
        dst += 8;
        hv = hv1;
    }
}

void __not_in_flash_func(encode_subpacket_hstx)(const data_packet_t *data_packet, uint32_t *dst) {
    for (int i = 0; i < 8; ++i) {
        uint32_t v = (data_packet->subpacket[0][i] << 0)  | (data_packet->subpacket[1][i] << 8) |
                     (data_packet->subpacket[2][i] << 16) | (data_packet->subpacket[3][i] << 24);
        uint32_t t = (v ^ (v >> 7)) & 0x00aa00aa;
        v = v ^ t ^ (t << 7);
        t = (v ^ (v >> 14)) & 0x0000cccc;
        v = v ^ t ^ (t << 14);
        // 01234567 89abcdef ghijklmn opqrstuv
        // 08go4cks 19hp5dlt 2aiq6emu 3bjr7fnv
        dst[0] |= (TERC4Syms_[(v >>  0) & 15] << 10) | (TERC4Syms_[(v >>  8) & 15] << 20);
        dst[1] |= (TERC4Syms_[(v >> 16) & 15] << 10) | (TERC4Syms_[(v >> 24) & 15] << 20);
        dst[2] |= (TERC4Syms_[(v >>  4) & 15] << 10) | (TERC4Syms_[(v >> 12) & 15] << 20);
        dst[3] |= (TERC4Syms_[(v >> 20) & 15] << 10) | (TERC4Syms_[(v >> 28) & 15] << 20);
        dst += 4;
    }
}

// encode consecutive data packets to the HDMI data island
// returns number of words written
uint32_t __not_in_flash_func(encode_data_islands_hstx)(uint32_t *dst, const data_packet_t *packet, size_t num_packets, int polarity) {
    if (num_packets == 0 || dst == NULL || packet == NULL) return 0;
    uint32_t* dst0 = dst;

    // leading guard band
    dst[0] = dst[1] = TERC4Syms_[polarity | 0b1100] | (0b0100110011 << 10) | (0b0100110011 << 20); dst += 2;

    // encode data packets
    for (int i = 0; i < num_packets; i++) {
        encode_header_hstx(packet, dst, polarity, i == 0);
        encode_subpacket_hstx(packet, dst);
        dst += W_DATA_PACKET; packet++;
    }

    // trailing guard band
    dst[0] = dst[1] = TERC4Syms_[polarity | 0b1100] | (0b0100110011 << 10) | (0b0100110011 << 20); dst += 2;

    return dst - dst0;
}

