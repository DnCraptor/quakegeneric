#include <stdint.h>
#include "xipstream.h"
#include "hardware/dma.h"
#include "hardware/structs/xip_ctrl.h"

static uint8_t xipstream_dma_ch = -1;

int xipstream_init()
{
    // alloc DMA channel
    xipstream_dma_ch = dma_claim_unused_channel(true);

    // setup it
    dma_channel_config cfg = dma_channel_get_default_config(xipstream_dma_ch);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_XIP_STREAM);
    dma_channel_set_config(xipstream_dma_ch, &cfg, false);
    dma_channel_set_read_addr(xipstream_dma_ch, (const void *) XIP_AUX_BASE, false);

    return 0;
}

int xipstream_start(void *dst, void *src, uint32_t words)
{
    if (xipstream_dma_ch == -1) {
        panic("xipstream_start(): DMA channel is not initialized!\n");
        return 1;
    }
    if (xipstream_is_running()) {
        panic("xipstream_start(): transfer in progress!\n");
        return 2;
    }
    if ((uintptr_t)src < XIP_BASE || (uintptr_t)src >= XIP_BASE + XIP_END) {
        panic("xipstream_start(): src=%08X outside XIP window!\n", src);
        return 3;
    }

    // drain XIP FIFO
    while (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY)) (void) xip_ctrl_hw->stream_fifo;

    // init FIFO
    xip_ctrl_hw->stream_addr = (uint32_t)src;
    xip_ctrl_hw->stream_ctr  = words;

    // and start the transfer
    dma_channel_set_write_addr    (xipstream_dma_ch, dst,  false);
    dma_channel_set_transfer_count(xipstream_dma_ch, words, true);

    return 0;
}

int xipstream_is_running()
{
    return dma_channel_is_busy(xipstream_dma_ch);
}

int xipstream_wait_blocking()
{
    dma_channel_wait_for_finish_blocking(xipstream_dma_ch);
    return 0;
}

int xipstream_abort() {
    dma_channel_abort(xipstream_dma_ch);
    xip_ctrl_hw->stream_ctr  = 0;   // should stop streaming read
    while (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY)) (void) xip_ctrl_hw->stream_fifo;
    return 0;
}
