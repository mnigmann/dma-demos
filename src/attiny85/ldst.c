#include <stdio.h>
#include <stdint.h>
#include <rpihw/rpihw.h>
#include "ldst.h"
#include "dma_chain.h"
#include "lut.h"


DMA_CB *cc_ldst(DMA_CTX *pctx, DMA_MEM_REF addlut, DMA_MEM_REF tmp, DMA_MEM_REF regfile, DMA_MEM_REF instr) {
    DMA_CTX *ctx = init_ctx(pctx);
    // Load the selected register (X, Y, or Z)
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 6, cc_ofs(regfile, 26), tmp);
    CC_REF(0).stride = 31<<16;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 1, cc_ofs(instr, 2), CC_RREF(1, tfr_len));
    CC_REF(0).stride = 0xfffefffe;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (3<<16), cc_ofs(tmp, 3), cc_ofs(tmp, 5));
    CC_REF(0).stride = 0xfffefffe;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (3<<16), cc_ofs(tmp, 3), cc_ofs(tmp, 5));
    cc_label(ctx, "reg", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, cc_ofs(tmp, 4), cc_ofs(CC_RREF(0, unused), 2)));

    // Load 0xffff into the adder if pre-decrement is selected
    CC_REF(1).unused = 0xffff0000;
    cc_label(ctx, "ofs", cc_lut(ctx, CC_RREF(1, unused), cc_ofs(instr, 1), CC_RREF(0, unused), 2, 2));

    cc_add16(ctx, addlut, cc_ofs(CC_DREF("reg", unused), 2), CC_DREF("ofs", unused), instr, tmp, CC_DREF("sum", unused));

    // Overwrite the address with the new address if using pre-decrement
    cc_mem2mem(ctx, 0, 1, cc_ofs(instr, 1), CC_RREF(1, tfr_len));
    cc_label(ctx, "sum", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16), CC_RREF(0, unused), cc_ofs(CC_DREF("reg", unused), 2)));

    // Store the updated address
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, cc_ofs(regfile, 30), CC_DREF("update", dest_ad));
    CC_REF(0).stride = 0x001ffffe;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 1, cc_ofs(instr, 3), CC_RREF(1, tfr_len));
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (3<<16), cc_ofs(regfile, 28), CC_DREF("update", dest_ad));
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (3<<16), cc_ofs(regfile, 26), CC_DREF("update", dest_ad));
    cc_label(ctx, "update", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_DREF("sum", unused), cc_ofs(regfile, 26)));

    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 64, addlut, tmp);
    cc_combined_shift(ctx, tmp, 2, cc_ofs(instr, 4), 5);
    cc_mem2mem(ctx, 0, 1, tmp, CC_DREF("reg", unused));
    // reg.unused contains the RAM address in the upper 16 bits and the register address in the lower 16 bits
    // This is copied backwards into the stride register for ld and forwards for st
    CC_REF(0).stride = 0xfffc;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 2, cc_ofs(CC_DREF("reg", unused), 2), CC_DREF("copy", stride));
    cc_mem2mem(ctx, 0, 1, cc_ofs(instr, 9), CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (3<<16), CC_DREF("reg", unused), CC_DREF("copy", stride));
    cc_label(ctx, "copy", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 1, cc_ofs(regfile, -1), cc_ofs(regfile, -1)));

    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

