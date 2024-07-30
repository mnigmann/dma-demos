#include <stdint.h>
#include <rpihw/rpihw.h>
#include "dma_chain.h"

#ifndef LUT_H
#define LUT_H

uint16_t *alloc_uncached_addlut(DMA_MEM_MAP *mp);
uint8_t *alloc_uncached_8to64(DMA_MEM_MAP *mp);
void cbs_lut8(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *lut, uint8_t *index, uint8_t *target, DMA_CB *next_cb);
DMA_CB *cc_lut(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF index, DMA_MEM_REF target, uint16_t size, uint16_t spacing);
DMA_CB *cc_convert_8to64(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF value, DMA_MEM_REF target);
DMA_CB *cc_convert_64to8(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF tmp, DMA_MEM_REF value, DMA_MEM_REF target);
DMA_CB *cc_combined_shift(DMA_CTX *pctx, DMA_MEM_REF lut, uint8_t size, DMA_MEM_REF value, uint8_t nbits);
DMA_CB *cc_add8(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF a, DMA_MEM_REF b, DMA_MEM_REF c, 
        DMA_MEM_REF tmp, DMA_MEM_REF sum);
DMA_CB *cc_add16(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF a, DMA_MEM_REF b, DMA_MEM_REF c, 
        DMA_MEM_REF tmp, DMA_MEM_REF sum);
DMA_CB *cc_add_pc(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF ofs, DMA_MEM_REF pc, DMA_MEM_REF tmp, DMA_MEM_REF sum);

#endif
