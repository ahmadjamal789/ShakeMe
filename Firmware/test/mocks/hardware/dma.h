#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t ctrl; } dma_channel_config;
enum dma_channel_transfer_size { DMA_SIZE_8 = 0, DMA_SIZE_16 = 1, DMA_SIZE_32 = 2 };
inline int dma_claim_unused_channel(bool) { return 0; }
inline dma_channel_config dma_channel_get_default_config(unsigned) { return {0}; }
inline void channel_config_set_transfer_data_size(dma_channel_config*, enum dma_channel_transfer_size) {}
inline void channel_config_set_dreq(dma_channel_config*, unsigned) {}
inline void channel_config_set_read_increment(dma_channel_config*, bool) {}
inline void channel_config_set_write_increment(dma_channel_config*, bool) {}
inline void dma_channel_configure(unsigned, const dma_channel_config*, volatile void*, const volatile void*, uint32_t, bool) {}
inline void dma_channel_wait_for_finish_blocking(unsigned) {}
