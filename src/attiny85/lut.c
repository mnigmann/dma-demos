#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <rpihw/rpihw.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "dma_chain.h"

#define DMA_CHAN 5

uint16_t *alloc_uncached_addlut(DMA_MEM_MAP *mp) {
    uint16_t *ptr = alloc_uncached_uint16(mp, 512);
    for (uint16_t i=0; i < 512; i++) {
        ptr[i] = i;
    }
    return ptr;
}

uint8_t *alloc_uncached_8to64(DMA_MEM_MAP *mp) {
    pad_uncached(mp, 8);
    uint8_t *ptr = alloc_uncached_uint8(mp, 8*256);
    for (int i=0; i < 256; i++) {
        for (int j=0; j < 8; j++) {
            ptr[8*i+j] = ((i & (1<<j)) ? 1 : 0);
        }
    }
    return ptr;
}

void cbs_lut8(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *lut, uint8_t *index, uint8_t *target, DMA_CB *next_cb) {
    cb_mem2mem(mp, cb, 0, 1, index, &(cb[1].stride), cb+1);
    cb_mem2mem(mp, cb+1, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 1, lut-1, target, next_cb);
    cb[1].stride = 0xffff<<16;
}

DMA_CB *cc_lut(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF index, DMA_MEM_REF target, uint16_t size, uint16_t spacing) {
    DMA_CTX *ctx = init_ctx(pctx);
    cc_mem2mem(ctx, 0, 1, index, CC_MREF(&(CC_REF(1).stride)));
    CC_REF(0).stride = (-size)<<16;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (spacing<<16) | size, cc_ofs(lut, -size*spacing), target);
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

DMA_CB *cc_convert_8to64(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF value, DMA_MEM_REF target) {
    return cc_lut(pctx, lut, value, target, 8, 8);
}

DMA_CB *cc_convert_64to8(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF tmp, DMA_MEM_REF value, DMA_MEM_REF target) {
    DMA_CTX *ctx = init_ctx(pctx);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2*256, lut, tmp);
    CC_REF(0).stride = ((0x10000 - 33)<<16);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (7<<16) | 1, value, CC_RREF(8, tfr_len));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (256<<16), cc_ofs(tmp, 256), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (128<<16), cc_ofs(tmp, 128), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (64<<16), cc_ofs(tmp, 64), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (32<<16), cc_ofs(tmp, 32), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (16<<16), cc_ofs(tmp, 16), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (8<<16), cc_ofs(tmp, 8), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (4<<16), cc_ofs(tmp, 4), tmp);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16), cc_ofs(tmp, 2), tmp);
    cc_mem2mem(ctx, 0, 1, tmp, target);
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

DMA_CB *cc_combined_shift(DMA_CTX *pctx, DMA_MEM_REF lut, uint8_t size, DMA_MEM_REF value, uint8_t nbits) {
    DMA_CTX *ctx = init_ctx(pctx);
    CC_REF(0).stride = ((0x10000 - 33)<<16);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, ((nbits-1)<<16) | 1, value, CC_RREF(nbits, tfr_len));
    for (uint8_t i=0; i < nbits; i++)
        cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (size << (15 + nbits - i)), cc_ofs(lut, (size<<(nbits - i - 1))), lut);
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

/*void cbs_combined_shift(DMA_MEM_MAP *mp, DMA_CB *cb, void *lut, uint8_t size, uint8_t *value, uint8_t nbits, DMA_CB *next_cb) {
    cb_mem2mem(mp, cb, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (nbits<<16) | 1, value, &(cb[nbits].tfr_len), cb+1);
    cb[0].stride = ((0x10000 - 33)<<16);
    for (uint8_t i=0; i < nbits; i++)
        cb_mem2mem(mp, cb+1+i, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (size << (15 + nbits - i)), lut+(size<<(nbits - i - 1)), lut, cb+2+i);
    cb[nbits].next_cb = (next_cb ? MEM_BUS_ADDR(mp, next_cb) : 0);
}

void cbs_inv_combined_shift(DMA_MEM_MAP *mp, DMA_CB *cb, void *lut, uint8_t size, uint8_t *value, uint8_t nbits, DMA_CB *next_cb) {
    cb_mem2mem(mp, cb, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (nbits<<16) | 1, value, &(cb[nbits].tfr_len), cb+1);
    cb[0].stride = ((0x10000 - 33)<<16);
    for (uint8_t i=0; i < nbits; i++)
        cb_mem2mem(mp, cb+1+i, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (size << (16 + i)), lut, lut+(size<<i), cb+2+i);
    cb[nbits].next_cb = (next_cb ? MEM_BUS_ADDR(mp, next_cb) : 0);
}*/

DMA_CB *cc_add8(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF a, DMA_MEM_REF b, DMA_MEM_REF c, 
        DMA_MEM_REF tmp, DMA_MEM_REF sum) {
    DMA_CTX *ctx = init_ctx(pctx);
    cc_imm2mem(ctx, 0, 4, CC_CREF("nocarry"), CC_DREF("loadc", next_cb));
    cc_mem2mem(ctx, 0, 1, c, CC_RREF(1, tfr_len));
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (4<<16), CC_CREF("carry"), CC_DREF("loadc", next_cb));
    CC_LABEL("loadnc", cc_mem2mem(ctx, 0, 1, a, CC_DREF("nocarry", stride)));
    CC_LABEL("loadc", cc_mem2mem(ctx, 0, 1, a, CC_DREF("carry", stride)));
    // Branch for no carry
    CC_REF(0).stride = 0xfe00<<16;
    CC_LABEL("nocarry", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 512, cc_ofs(lut, -1024), tmp));
    cc_goto(ctx, CC_CREF("addb"));
    // Branch for carry
    CC_REF(0).stride = 0xfe00<<16;
    CC_LABEL("carry", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 512, cc_ofs(lut, -1022), tmp));
    // Add b
    CC_LABEL("addb", cc_mem2mem(ctx, 0, 1, b, CC_RREF(1, stride)));
    CC_REF(0).stride = 0xfffe<<16;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 2, cc_ofs(tmp, -4), sum);
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

DMA_CB *cc_add16(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF a, DMA_MEM_REF b, DMA_MEM_REF c, 
        DMA_MEM_REF tmp, DMA_MEM_REF sum) {
    DMA_CTX *ctx = init_ctx(pctx);
    cc_add8(ctx, lut, a, b, c, tmp, sum);
    cc_add8(ctx, lut, cc_ofs(a, 1), cc_ofs(b, 1), cc_ofs(sum, 1), tmp, cc_ofs(sum, 1));
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

DMA_CB *cc_add_pc(DMA_CTX *pctx, DMA_MEM_REF lut, DMA_MEM_REF ofs, DMA_MEM_REF pc, DMA_MEM_REF tmp, DMA_MEM_REF sum) {
    DMA_CTX *ctx = init_ctx(pctx);
    DMA_MEM_REF zero_ref = CC_RREF(3, unused);
    DMA_MEM_REF tmp_ref = CC_RREF(4, unused);
    CC_REF(3).unused = 0;
    cc_add8(ctx, lut, ofs, pc, zero_ref, tmp, sum);
    cc_add8(ctx, lut, cc_ofs(ofs, 1), cc_ofs(pc, 1), cc_ofs(sum, 1), tmp, cc_ofs(sum, 1));
    cc_add8(ctx, lut, cc_ofs(ofs, 2), zero_ref, cc_ofs(sum, 2), tmp, cc_ofs(sum, 2));
    cc_add8(ctx, lut, cc_ofs(ofs, 3), zero_ref, cc_ofs(sum, 3), tmp, tmp_ref);
    cc_mem2mem(ctx, 0, 1, tmp_ref, cc_ofs(sum, 3));
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

/*void cbs_add16(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint16_t *a, uint16_t *b, uint8_t *c, uint16_t *tmp, uint32_t *sum, DMA_CB *next_cb) {
    cbs_add8(mp, cb, lut, (uint8_t*)a, (uint8_t*)b, c, tmp, (uint16_t*)sum, cb+9);
    cbs_add8(mp, cb+9, lut, ((uint8_t*)a)+1, ((uint8_t*)b)+1, ((uint8_t*)sum)+1, tmp, (uint16_t*)(((uint8_t*)sum)+1), next_cb);
}

void cbs_add_pc(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint32_t ofs, uint16_t *pc, uint16_t *tmp, uint32_t *sum, DMA_CB *next_cb) {
    cbs_add8(mp, cb, lut, (uint8_t*)(&(cb[1].unused)), (uint8_t*)pc, (uint8_t*)(&(cb[3].unused)), tmp, (uint16_t*)sum, cb+9);
    cb[1].unused = ofs;
    cb[3].unused = 0;
    cbs_add8(mp, cb+9, lut, (uint8_t*)(&(cb[1].unused))+1, ((uint8_t*)pc)+1, ((uint8_t*)sum)+1, tmp, (uint16_t*)(((uint8_t*)sum)+1), cb+18);
    cbs_add8(mp, cb+18, lut, (uint8_t*)(&(cb[1].unused))+2, (uint8_t*)(&(cb[3].unused)), ((uint8_t*)sum)+2, tmp, (uint16_t*)(((uint8_t*)sum)+2), cb+27);
    cbs_add8(mp, cb+27, lut, (uint8_t*)(&(cb[1].unused))+3, (uint8_t*)(&(cb[3].unused)), ((uint8_t*)sum)+3, tmp, (uint16_t*)(&(cb[4].unused)), cb+36);
    cb_mem2mem(mp, cb+36, 0, 1, &(cb[4].unused), ((uint8_t*)sum)+3, next_cb);
}*/

/*DMA_MEM_MAP uc_mem;

void done(int sig) {
    stop_dma(DMA_CHAN);
    unmap_uncached_mem(&uc_mem);
    terminate(0);
    exit(0);
}

int main() {
    map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE);
    map_uncached_mem(&uc_mem, 16*PAGE_SIZE);
    memset(uc_mem.virt, 0, 16*PAGE_SIZE);
    
    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 64);
    uint16_t *addlut = alloc_uncached_addlut(&uc_mem);
    uint8_t *lut_8to64 = alloc_uncached_8to64(&uc_mem);
    uint16_t *tmp = alloc_uncached_uint16(&uc_mem, 256);
    uint8_t *tmp8 = alloc_uncached_uint8(&uc_mem, 256);
    uint32_t *sum = alloc_uncached_uint32(&uc_mem, 1);
    uint16_t *a = alloc_uncached_uint16(&uc_mem, 1);
    uint16_t *b = alloc_uncached_uint16(&uc_mem, 1);
    uint8_t *c = alloc_uncached_uint8(&uc_mem, 1);
    a[0] = 27;
    b[0] = 25677;
    c[0] = 0;
    printf("sum should be %04x\n", a[0] + b[0]);
    //cbs_convert_8to32(&uc_mem, cbs, lut_8to32, c, tmp8, cbs+2);
    //cbs_convert_32to8(&uc_mem, cbs+2, addlut, tmp, tmp8, tmp8+8, 0);
    DMA_CTX *ctx = init_ctx(NULL);
    ctx->mp = &uc_mem;
    ctx->start_cb = cbs;
    //cc_add16(&ctx, CC_MREF(addlut), CC_MREF(a), CC_MREF(b), CC_MREF(c), CC_MREF(tmp), CC_MREF(sum));
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(a), CC_MREF(tmp8));
    cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(tmp8), CC_MREF(tmp8+8));
    for (int i=0; i < ctx->n_cbs; i++) {
        printf("%08x: SRCE_AD %08x, DEST_AD %08x, TFR_LEN %08x, STRIDE %08x, UNUSED %08x\n", MEM_BUS_ADDR(&uc_mem, cbs+i), 
               cbs[i].srce_ad, cbs[i].dest_ad, cbs[i].tfr_len, cbs[i].stride, cbs[i].unused);
    }
    cc_clean(NULL, ctx);

    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    clock_t start_time = clock();
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    clock_t end_time = clock();
    printf("result is");
    for (int i=0; i < 8; i++) printf(" %02x", tmp8[i]);
    printf("\n");
    printf("converted: %d\n", tmp8[8]);
    printf("Completed in %luus\n", end_time - start_time);

    done(0);
    return 0;
}*/
