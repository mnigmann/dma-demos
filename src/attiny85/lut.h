#include <stdint.h>
#include <rpihw/rpihw.h>
#include "dma_chain.h"

#ifndef LUT_H
#define LUT_H

uint16_t *alloc_uncached_addlut(DMA_MEM_MAP *mp);
uint8_t *alloc_uncached_8to32(DMA_MEM_MAP *mp);
void cbs_lut8(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *lut, uint8_t *index, uint8_t *target, DMA_CB *next_cb);
DMA_CTX cbs_lut(DMA_MEM_MAP *mp, DMA_CB *cb, void *lut, uint8_t *index, void *target, uint16_t size, uint16_t spacing, DMA_CB *next_cb);
DMA_CTX cbs_convert_8to32(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *lut, uint8_t *value, uint8_t *target, DMA_CB *next_cb);
DMA_CTX cbs_convert_32to8(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint16_t *tmp, uint8_t *value, uint8_t *target, DMA_CB *next_cb);
DMA_CTX cbs_combined_shift(DMA_MEM_MAP *mp, DMA_CB *cb, void *lut, uint8_t size, uint8_t *value, uint8_t nbits, DMA_CB *next_cb);
void cbs_inv_combined_shift(DMA_MEM_MAP *mp, DMA_CB *cb, void *lut, uint8_t size, uint8_t *value, uint8_t nbits, DMA_CB *next_cb);
DMA_CTX cbs_add8(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint8_t *a, uint8_t *b, uint8_t *c, uint16_t *tmp, uint16_t *sum, DMA_CB *next_cb);
DMA_CTX cbs_add16(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint16_t *a, uint16_t *b, uint8_t *c, uint16_t *tmp, uint32_t *sum, DMA_CB *next_cb);
DMA_CTX cbs_add_pc(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint32_t ofs, uint16_t *pc, uint16_t *tmp, uint32_t *sum, DMA_CB *next_cb);

#endif
