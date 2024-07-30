#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <rpihw/rpihw.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "alu.h"
#include "dma_chain.h"

#define DMA_CHAN 5

void load_bits(uint8_t val, uint8_t *buf) {
    for (uint8_t i=0; i < 8; i++) {
        if (val & 1) buf[i] = 1;
        else buf[i] = 0;
        val = val >> 1;
    }
}

uint8_t extract_bits(uint8_t *buf) {
    uint8_t res = 0;
    for (uint8_t i=0; i < 8; i++) res |= (buf[i] << i);
    return res;
}

uint32_t *alloc_uncached_alucon(DMA_MEM_MAP *mp, DMA_CB *alu_root, uint8_t mode) {
    uint32_t *alucon = alloc_uncached_uint32(mp, 8);
    switch (mode) {
        case ALUCON_ZNVS:
            for (uint8_t i=0; i < 7; i++) alucon[i] = MEM_BUS_ADDR(mp, alu_root);
            alucon[7] = alu_root[15].next_cb;
            break;
        case ALUCON_CZNVS:
            for (uint8_t i=0; i < 7; i++) alucon[i] = MEM_BUS_ADDR(mp, alu_root+13);
            alucon[7] = MEM_BUS_ADDR(mp, alu_root+14);
            break;
        case ALUCON_CZNVSH:
        default:
            for (uint8_t i=0; i < 7; i++) alucon[i] = MEM_BUS_ADDR(mp, alu_root+13);
            alucon[7] = MEM_BUS_ADDR(mp, alu_root+14);
            alucon[3] = MEM_BUS_ADDR(mp, alu_root+12);
            break;
    }
    return alucon;
}

DMA_CB *cc_alu8(DMA_CTX *pctx, DMA_MEM_REF alu_lut, DMA_MEM_REF rd, DMA_MEM_REF rr, DMA_MEM_REF carry, 
                DMA_MEM_REF alucon_sr, DMA_MEM_REF alu_lut_sr, DMA_MEM_REF sreg) {
    DMA_CTX *ctx = init_ctx(pctx);
    // Load alu_lut_sr
    cc_label(ctx, "loop", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 12, alu_lut, alu_lut_sr));
    // Add bit from rd
    cc_mem2mem(ctx, 0, 1, rd, CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8<<16, cc_ofs(alu_lut_sr, 3), alu_lut_sr);
    // Add bit from rr
    cc_mem2mem(ctx, 0, 1, rr, CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8<<16, cc_ofs(alu_lut_sr, 3), alu_lut_sr);
    // Add carry bit
    cc_mem2mem(ctx, 0, 1, carry, CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8<<16, cc_ofs(alu_lut_sr, 3), alu_lut_sr);
    // Load pointer from alucon_sr into next_cb
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, alucon_sr, CC_DREF("jump", next_cb));
    // Shift alucon_sr
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 28, cc_ofs(alucon_sr, 4), alucon_sr);
    // Shift rd
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7, cc_ofs(rd, 1), rd);
    // Shift rr
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7, cc_ofs(rr, 1), rr);
    // Store result
    cc_label(ctx, "jump", cc_mem2mem(ctx, 0, 1, alu_lut_sr, cc_ofs(rd, 7)));
    // ALU_BW:
    //     goto cb[0]
    // ALU_STORE_HALF_CARRY:
    //     Store carry to H bit
    cc_mem2mem(ctx, 0, 1, cc_ofs(alu_lut_sr, 1), cc_ofs(sreg, 5));
    // ALU:
    //     Store carry
    //     goto cb[0]
    cc_mem2mem(ctx, 0, 1, cc_ofs(alu_lut_sr, 1), carry);
    cc_goto(ctx, CC_CREF("loop"));
    // ALU_STORE_CARRY:
    //     Store carry to C bit
    cc_mem2mem(ctx, 0, 1, cc_ofs(alu_lut_sr, 1), sreg);
    //     Store carry
    //     break
    cc_mem2mem(ctx, 0, 1, cc_ofs(alu_lut_sr, 1), carry);
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

DMA_CB *cc_inv(DMA_CTX *pctx, DMA_MEM_REF src, DMA_MEM_REF dest, uint8_t nbits) {
    DMA_CTX *ctx = init_ctx(pctx);
    CC_REF(0).unused = 0;
    CC_REF(0).stride = (31 << 16);
    cc_label(ctx, "zero", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, ((nbits-1)<<16) | 1, src, CC_RREF(2, tfr_len)));
    CC_REF(0).stride = 0xffff;
    CC_REF(0).unused = 0x01010101;
    cc_mem2mem(ctx, DMA_CB_DEST_INC | DMA_CB_SRCE_INC | DMA_TDMODE, ((nbits-1)<<16) | 1, CC_RREF(0, unused), dest);
    for (uint8_t i=0; i < nbits; i++) {
        cc_mem2mem(ctx, 0, 0, CC_DREF("zero", unused), cc_ofs(dest, i));
    }
    cc_ret(ctx);
    return cc_clean(pctx, ctx);
}

/*DMA_MEM_MAP uc_mem;

void done(int sig) {
    stop_dma(DMA_CHAN);
    unmap_uncached_mem(&uc_mem);
    terminate(0);
    exit(0);
}

int main(void) {
    signal(SIGINT, done);
    map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE);
    map_uncached_mem(&uc_mem, 16*PAGE_SIZE);

    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 32);
    uint8_t *rd = alloc_uncached_uint8(&uc_mem, 8);
    load_bits(0b11011001, rd);
    uint8_t *rr = alloc_uncached_uint8(&uc_mem, 8);
    load_bits(0b10101011, rr);
    printf("rd: %02x, rr: %02x, sum: %04x\n", extract_bits(rd), extract_bits(rr), (uint16_t)extract_bits(rd) + extract_bits(rr));
    uint32_t *alucon = alloc_uncached_alucon(&uc_mem, cbs+1, ALUCON_CZNVSH);
    uint32_t *alucon_sr = alloc_uncached_uint32(&uc_mem, 8);
    uint8_t *alu_lut = alloc_uncached_uint8(&uc_mem, 12);
    alu_lut[0] = 0; alu_lut[1] = 0; alu_lut[2] = 0;
    alu_lut[3] = 1; alu_lut[4] = 0; alu_lut[5] = 1;
    alu_lut[6] = 0; alu_lut[7] = 1; alu_lut[8] = 1;
    alu_lut[9] = 1; alu_lut[10] = 1; alu_lut[11] = 1;
    uint8_t *alu_lut_sr = alloc_uncached_uint8(&uc_mem, 12);
    uint8_t *sreg = alloc_uncached_uint8(&uc_mem, 8);
    uint8_t *carry = alloc_uncached_uint8(&uc_mem, 1);
    sreg[0] = 0;

    DMA_CTX ctx;
    ctx.mp = &uc_mem;
    ctx.start_cb = cbs;
    ctx.n_cbs = ctx.n_labels = ctx.n_links = 0;
    ctx.labels = malloc(32*sizeof(DMA_MEM_LABEL));
    ctx.links = malloc(16*sizeof(DMA_MEM_LINK));

    cc_mem2mem(&ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(alucon), CC_MREF(alucon_sr));
    //printf("alucon final part: %08x, alu_root %08x\n", alucon[7], MEM_BUS_ADDR(&uc_mem, cbs+1));
    cc_alu8(&ctx, CC_MREF(alu_lut), CC_MREF(rd), CC_MREF(rr), CC_MREF(carry), CC_MREF(alucon_sr), CC_MREF(alu_lut_sr), CC_MREF(sreg));

    free(ctx.labels);
    free(ctx.links);


    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    clock_t start_time = clock();
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    clock_t end_time = clock();
    printf("rd: %02x, rr: %02x, carry: %01x\n", extract_bits(rd), extract_bits(rr), sreg[0]);
    printf("sreg: %02x\n", extract_bits(sreg));
    printf("Completed in %luus\n", end_time - start_time);

    done(0);
}*/
