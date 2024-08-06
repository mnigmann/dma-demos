#include <rpihw/rpihw.h>
#include "dma_chain.h"

#ifndef LDST_H
#define LDST_H

DMA_CB *cc_ldst(DMA_CTX *pctx, DMA_MEM_REF addlut, DMA_MEM_REF tmp, DMA_MEM_REF regfile, DMA_MEM_REF instr);

#endif
