#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <rpihw/rpihw.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define DMA_CHAN 5

#define ALUCON_CZNVSH   0
#define ALUCON_ZNVS     1
#define ALUCON_CZNVS    2

DMA_MEM_MAP uc_mem;

void done(int sig) {
    stop_dma(DMA_CHAN);
    unmap_uncached_mem(&uc_mem);
    terminate(0);
    exit(0);
}

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

uint32_t cbs_alu8(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *alu_lut, uint8_t *rd, uint8_t *rr, uint8_t *carry, 
                  uint32_t *alucon_sr, uint8_t *alu_lut_sr, uint8_t *sreg, DMA_CB *next_cb) {
    // Load alu_lut_sr
    cb_mem2mem(mp, cb, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 12, alu_lut, alu_lut_sr, cb+1);
    // Add bit from rd
    cb_mem2mem(mp, cb+1, 0, 1, rd, &(cb[2].tfr_len), cb+2);
    cb_mem2mem(mp, cb+2, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 9<<16, alu_lut_sr+3, alu_lut_sr, cb+3);
    // Add bit from rr
    cb_mem2mem(mp, cb+3, 0, 1, rr, &(cb[4].tfr_len), cb+4);
    cb_mem2mem(mp, cb+4, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 9<<16, alu_lut_sr+3, alu_lut_sr, cb+5);
    // Add carry bit
    cb_mem2mem(mp, cb+5, 0, 1, carry, &(cb[6].tfr_len), cb+6);
    cb_mem2mem(mp, cb+6, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 9<<16, alu_lut_sr+3, alu_lut_sr, cb+7);
    // Load pointer from alucon_sr into next_cb
    cb_mem2mem(mp, cb+7, 0, 4, alucon_sr, &(cb[11].next_cb), cb+8);
    // Shift alucon_sr
    cb_mem2mem(mp, cb+8, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 28, alucon_sr+1, alucon_sr, cb+9);
    // Shift rd
    cb_mem2mem(mp, cb+9, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7, rd+1, rd, cb+10);
    // Shift rr
    cb_mem2mem(mp, cb+10, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7, rr+1, rr, cb+11);
    // Store result
    cb_mem2mem(mp, cb+11, 0, 1, alu_lut_sr, rd+7, cb);
    // ALU_BW:
    //     goto cb[0]
    // ALU_STORE_HALF_CARRY:
    //     Store carry to H bit
    cb_mem2mem(mp, cb+12, 0, 1, alu_lut_sr+1, sreg+5, cb+13);
    // ALU:
    //     Store carry
    //     goto cb[0]
    cb_mem2mem(mp, cb+13, 0, 1, alu_lut_sr+1, carry, cb);
    // ALU_STORE_CARRY:
    //     Store carry to C bit
    cb_mem2mem(mp, cb+14, 0, 1, alu_lut_sr+1, sreg, cb+15);
    //     Store carry
    //     break
    cb_mem2mem(mp, cb+15, 0, 1, alu_lut_sr+1, carry, next_cb);
    return 16;
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

    cb_mem2mem(&uc_mem, cbs, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, alucon, alucon_sr, cbs+1);
    printf("alucon final part: %08x, alu_root %08x\n", alucon[7], MEM_BUS_ADDR(&uc_mem, cbs+1));
    cbs_alu8(&uc_mem, cbs+1, alu_lut, rd, rr, carry, alucon_sr, alu_lut_sr, sreg, 0);


    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    clock_t start_time = clock();
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    clock_t end_time = clock();
    printf("rd: %02x, rr: %02x, carry: %01x\n", extract_bits(rd), extract_bits(rr), sreg[0]);
    printf("sreg: %02x\n", extract_bits(sreg));
    printf("Completed in %luus\n", end_time - start_time);

    done(0);
}
