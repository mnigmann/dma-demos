#include <stdint.h>
#include <rpihw/rpihw.h>
#include "dma_chain.h"

#ifndef ALU_H
#define ALU_H

#define ALUCON_CZNVSH   0
#define ALUCON_ZNVS     1
#define ALUCON_CZNVS    2

void load_bits(uint8_t val, uint8_t *buf);
uint8_t extract_bits(uint8_t *buf);
uint32_t *alloc_uncached_alucon(DMA_MEM_MAP *mp, DMA_CB *alu_root, uint8_t mode);
DMA_CB *cc_alu8(DMA_CTX *pctx, DMA_MEM_REF alu_lut, DMA_MEM_REF rd, DMA_MEM_REF rr, DMA_MEM_REF carry, 
                DMA_MEM_REF alucon_sr, DMA_MEM_REF alu_lut_sr, DMA_MEM_REF sreg);
DMA_CB *cc_inv(DMA_CTX *pctx, DMA_MEM_REF src, DMA_MEM_REF dest, uint8_t nbits);

#endif
