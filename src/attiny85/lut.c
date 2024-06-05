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

uint16_t *alloc_uncached_addlut(DMA_MEM_MAP *mp) {
    uint16_t *ptr = alloc_uncached_uint16(mp, 512);
    for (uint16_t i=0; i < 512; i++) {
        ptr[i] = i;
    }
    return ptr;
}

uint8_t *alloc_uncached_8to32(DMA_MEM_MAP *mp) {
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

void cbs_lut(DMA_MEM_MAP *mp, DMA_CB *cb, void *lut, uint8_t *index, void *target, uint16_t size, uint16_t spacing, DMA_CB *next_cb) {
    cb_mem2mem(mp, cb, 0, 1, index, &(cb[1].stride), cb+1);
    cb_mem2mem(mp, cb+1, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (spacing<<16) | size, lut-size*spacing, target, next_cb);
    cb[1].stride = (-size)<<16;
}

void cbs_convert_8to32(DMA_MEM_MAP *mp, DMA_CB *cb, uint8_t *lut, uint8_t *value, uint8_t *target, DMA_CB *next_cb) {
    cbs_lut(mp, cb, lut, value, target, 8, 8, next_cb);
}

void cbs_convert_32to8(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint16_t *tmp, uint8_t *value, uint8_t *target, DMA_CB *next_cb) {
    cb_mem2mem(mp, cb, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2*256, lut, tmp, cb+1);
    cb_mem2mem(mp, cb+1, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (8<<16) | 1, value, &(cb[9].tfr_len), cb+2);
    cb[1].stride = ((0x10000 - 33)<<16);
    cb_mem2mem(mp, cb+2, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (256<<16), tmp+128, tmp, cb+3);
    cb_mem2mem(mp, cb+3, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (128<<16), tmp+64, tmp, cb+4);
    cb_mem2mem(mp, cb+4, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (64<<16), tmp+32, tmp, cb+5);
    cb_mem2mem(mp, cb+5, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (32<<16), tmp+16, tmp, cb+6);
    cb_mem2mem(mp, cb+6, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (16<<16), tmp+8, tmp, cb+7);
    cb_mem2mem(mp, cb+7, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (8<<16), tmp+4, tmp, cb+8);
    cb_mem2mem(mp, cb+8, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (4<<16), tmp+2, tmp, cb+9);
    cb_mem2mem(mp, cb+9, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16), tmp+1, tmp, cb+10);
    cb_mem2mem(mp, cb+10, 0, 1, tmp, target, next_cb);
}

void cbs_add8(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint8_t *a, uint8_t *b, uint8_t *c, uint16_t *tmp, uint16_t *sum, DMA_CB *next_cb) {
    cb_mem2mem(mp, cb, 0, 4, &(cb[0].unused), &(cb[4].next_cb), cb+1);
    cb[0].unused = MEM_BUS_ADDR(mp, cb+5);
    cb_mem2mem(mp, cb+1, 0, 1, c, &(cb[2].tfr_len), cb+2);
    cb_mem2mem(mp, cb+2, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (4<<16), &(cb[2].unused), &(cb[4].next_cb), cb+3);
    cb[2].unused = MEM_BUS_ADDR(mp, cb+6);
    cb_mem2mem(mp, cb+3, 0, 1, a, &(cb[5].stride), cb+4);
    cb_mem2mem(mp, cb+4, 0, 1, a, &(cb[6].stride), cb+5);
    // Branch for no carry
    cb_mem2mem(mp, cb+5, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 512, lut-512, tmp, cb+7);
    cb[5].stride = 0xfe00<<16;
    // Branch for carry
    cb_mem2mem(mp, cb+6, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 512, lut-511, tmp, cb+7);
    cb[6].stride = 0xfe00<<16;
    // Add b
    cb_mem2mem(mp, cb+7, 0, 1, b, &(cb[8].stride), cb+8);
    cb_mem2mem(mp, cb+8, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 2, tmp-2, sum, next_cb);
    cb[8].stride = 0xfffe<<16;
}

void cbs_add16(DMA_MEM_MAP *mp, DMA_CB *cb, uint16_t *lut, uint16_t *a, uint16_t *b, uint8_t *c, uint16_t *tmp, uint32_t *sum, DMA_CB *next_cb) {
    cbs_add8(mp, cb, lut, (uint8_t*)a, (uint8_t*)b, c, tmp, (uint16_t*)sum, cb+9);
    cbs_add8(mp, cb+9, lut, ((uint8_t*)a)+1, ((uint8_t*)b)+1, ((uint8_t*)sum)+1, tmp, (uint16_t*)(((uint8_t*)sum)+1), next_cb);
}

int main() {
    map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE);
    map_uncached_mem(&uc_mem, 16*PAGE_SIZE);
    memset(uc_mem.virt, 0, 16*PAGE_SIZE);
    
    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 64);
    uint16_t *addlut = alloc_uncached_addlut(&uc_mem);
    uint8_t *lut_8to32 = alloc_uncached_8to32(&uc_mem);
    uint16_t *tmp = alloc_uncached_uint16(&uc_mem, 256);
    uint8_t *tmp8 = alloc_uncached_uint8(&uc_mem, 256);
    uint32_t *sum = alloc_uncached_uint32(&uc_mem, 1);
    uint16_t *a = alloc_uncached_uint16(&uc_mem, 1);
    uint16_t *b = alloc_uncached_uint16(&uc_mem, 1);
    uint8_t *c = alloc_uncached_uint8(&uc_mem, 1);
    a[0] = 7344;
    b[0] = 12532;
    c[0] = 234;
    cbs_convert_8to32(&uc_mem, cbs, lut_8to32, c, tmp8, cbs+2);
    cbs_convert_32to8(&uc_mem, cbs+2, addlut, tmp, tmp8, tmp8+8, 0);

    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    clock_t start_time = clock();
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    clock_t end_time = clock();
    printf("sum is");
    for (int i=0; i < 8; i++) printf(" %02x", tmp8[i]);
    printf("\nextracted: %d\n", tmp8[8]);
    printf("Completed in %luus\n", end_time - start_time);

    done(0);
    return 0;
}
