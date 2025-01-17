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
void populate_alucon(DMA_MEM_MAP *mp, uint32_t *alucon, DMA_CB *alu_root, uint8_t mode);
DMA_CB *cc_alu8(DMA_CTX *pctx, DMA_MEM_REF alu_lut, DMA_MEM_REF rd, DMA_MEM_REF rr, DMA_MEM_REF carry, 
                DMA_MEM_REF alucon_sr, DMA_MEM_REF alu_lut_sr, DMA_MEM_REF sreg);
DMA_CB *cc_unary_alu(DMA_CTX *pctx, DMA_MEM_REF unary_lut, DMA_MEM_REF lut_8to64, DMA_MEM_REF tmp, DMA_MEM_REF instr, DMA_MEM_REF rd, DMA_MEM_REF rr, 
                     DMA_MEM_REF sreg, DMA_CB *aluadd, DMA_CB *alusub, DMA_CB *bitwrite);
DMA_CB *cc_inv(DMA_CTX *pctx, DMA_MEM_REF src, DMA_MEM_REF dest, uint8_t nbits);

#endif
