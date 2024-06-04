#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <rpihw/rpihw.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define DMA_CHAN 5

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

int main(void) {
    map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE);
    map_uncached_mem(&uc_mem, 16*PAGE_SIZE);

    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 32);
    uint8_t *rd = alloc_uncached_uint8(&uc_mem, 8);
    load_bits(0b01010001, rd);
    uint8_t *rr = alloc_uncached_uint8(&uc_mem, 8);
    load_bits(0b00101011, rr);
    uint8_t *alu_lut = alloc_uncached_uint8(&uc_mem, 12);
    alu_lut[0] = 0; alu_lut[1] = 0; alu_lut[2] = 0;
    alu_lut[3] = 1; alu_lut[4] = 0; alu_lut[5] = 1;
    alu_lut[6] = 0; alu_lut[7] = 1; alu_lut[8] = 1;
    alu_lut[9] = 1; alu_lut[10] = 1; alu_lut[11] = 1;
    uint8_t *alu_lut_sr = alloc_uncached_uint8(&uc_mem, 12);
    uint32_t *alucon = alloc_uncached_uint32(&uc_mem, 8);
    for (uint8_t i=0; i < 7; i++) alucon[i] = MEM_BUS_ADDR(&uc_mem, cbs+1);
    alucon[7] = 0;
    uint32_t *alucon_sr = alloc_uncached_uint32(&uc_mem, 8);
    uint8_t *carry = alloc_uncached_uint8(&uc_mem, 1);
    carry[0] = 0;

    cb_mem2mem(&uc_mem, cbs, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, alucon, alucon_sr, cbs+1);
    cb_mem2mem(&uc_mem, cbs+1, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 12, alu_lut, alu_lut_sr, cbs+2);
    cb_mem2mem(&uc_mem, cbs+2, 0, 1, rd, &(cbs[3].tfr_len), cbs+3);
    cb_mem2mem(&uc_mem, cbs+3, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 9<<16, alu_lut_sr+3, alu_lut_sr, cbs+4);
    cb_mem2mem(&uc_mem, cbs+4, 0, 1, rr, &(cbs[5].tfr_len), cbs+5);
    cb_mem2mem(&uc_mem, cbs+5, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 9<<16, alu_lut_sr+3, alu_lut_sr, cbs+6);
    cb_mem2mem(&uc_mem, cbs+6, 0, 1, carry, &(cbs[7].tfr_len), cbs+7);
    cb_mem2mem(&uc_mem, cbs+7, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 9<<16, alu_lut_sr+3, alu_lut_sr, cbs+8);
    // Load pointer from alucon_sr into next_cb
    cb_mem2mem(&uc_mem, cbs+8, 0, 4, alucon_sr, &(cbs[12].next_cb), cbs+9);
    // Shift alucon_sr
    cb_mem2mem(&uc_mem, cbs+9, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 28, alucon_sr+1, alucon_sr, cbs+10);
    // Store carry
    cb_mem2mem(&uc_mem, cbs+10, 0, 1, alu_lut_sr+1, carry, cbs+11);
    // Shift rd and rr
    cb_mem2mem(&uc_mem, cbs+11, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 15, rd+1, rd, cbs+12);
    // Store result
    cb_mem2mem(&uc_mem, cbs+12, 0, 1, alu_lut_sr, rd+7, cbs+1);


    for (uint8_t i=0; i < 13; i++) {
        for (uint8_t j=0; j < 8; j++) printf("%02x ", rd[i*8+j]);
        printf("\n");
    }
    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    clock_t start_time = clock();
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    clock_t end_time = clock();
    printf("After:\n");
    for (uint8_t i=0; i < 13; i++) {
        for (uint8_t j=0; j < 8; j++) printf("%02x ", rd[i*8+j]);
        printf("\n");
    }
    printf("%02x %02x %02x, %08x, %08x, %08x\n", alu_lut_sr[0], alu_lut_sr[1], alu_lut_sr[2], alucon[0], alucon_sr[0], MEM_BUS_ADDR(&uc_mem, alucon));
    printf("rd: %02x, rr: %02x, carry: %01x\n", extract_bits(rd), extract_bits(rr), carry[0]);
    printf("Completed in %luus\n", end_time - start_time);

    done(0);
}
