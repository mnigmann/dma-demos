#include <stdint.h>
#include <rpihw/rpihw.h>

#ifndef ALU_H
#define ALU_H

#define ALUCON_CZNVSH   0
#define ALUCON_ZNVS     1
#define ALUCON_CZNVS    2

void load_bits(uint8_t val, uint8_t *buf);
uint8_t extract_bits(uint8_t *buf);
uint32_t *alloc_uncached_alucon(DMA_MEM_MAP *mp, DMA_CB *alu_root, uint8_t mode);
uint32_t cbs_alu8(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *alu_lut, uint8_t *rd, uint8_t *rr, uint8_t *carry,
                  uint32_t *alucon_sr, uint8_t *alu_lut_sr, uint8_t *sreg, DMA_CB *next_cb);
uint32_t cbs_inv(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *src, uint8_t *dest, uint8_t nbits, DMA_CB *next_cb);

#endif
