#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <rpihw/rpihw.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct STATE_s {
    uint8_t zero_action;
    struct STATE_s *zero_state;
    uint8_t one_action;
    struct STATE_s *one_state;
} STATE;

DMA_MEM_MAP uc_mem;
STATE *states = NULL;

#define DMA_CHAN        5

#define TAPE_SIZE       128
#define N_STATES        11

#define ACTION_LEFT     0b00001
#define ACTION_RIGHT    0b00010
#define ACTION_ZERO     0b00100
#define ACTION_ONE      0b01000

#define ADDR_TO_STATE(ptr) ((ptr)==0 ? -1 : ((ptr)-states))


void done(int sig) {
    stop_dma(DMA_CHAN);
    if (states) free(states);
    unmap_uncached_mem(&uc_mem);
    terminate(0);
    exit(0);
}

void add_state(STATE *s, uint8_t zero_action, STATE *zero_state, uint8_t one_action, STATE *one_state) {
    s->zero_action = zero_action;
    s->zero_state = zero_state;
    s->one_action = one_action;
    s->one_state = one_state;
}

void print_action(uint8_t ac, int next) {
    printf("[");
    if (ac & (ACTION_ZERO | ACTION_ONE)) {
        printf("write ");
        if (ac & ACTION_ZERO) printf("0");
        if (ac & ACTION_ONE) printf("1");
        printf(", ");
    }
    if (ac & (ACTION_LEFT | ACTION_RIGHT)) {
        printf("move ");
        if (ac & ACTION_LEFT) printf("L");
        if (ac & ACTION_RIGHT) printf("R");
        printf(", ");
    }
    if (next >= 0) {
        printf("goto %d]", next);
    } else {
        printf("halt]");
    }
}

void print_tape(uint32_t *tape, uint32_t n) {
    for (uint32_t i=0; i < n; i++) {
        if (tape[TAPE_SIZE - n + i]) printf("1");
        else printf("0");
    }
    printf(" ");
    for (uint32_t i=0; i < n; i++) {
        if (tape[i]) printf("1");
        else printf("0");
    }
    printf("\n");
}


// For each state, we need
//  * One CB to load the value from the tape
//  * One CB to update the branch
//  * One CB to reset the branch
//  * One CB to load the value for 0
//  * Two CBs to shift the tape for 0
//  * One CB to load the value for 1
//  * Two CBs to shift the tape for 1
// Total of 9 CBs

int main() {
    signal(SIGINT, done);
    
    map_periph(&dma_regs, (void *)(DMA_BASE), PAGE_SIZE);
    map_uncached_mem(&uc_mem, 2*PAGE_SIZE);

    // Turing machine setup
    STATE *states = malloc(N_STATES*sizeof(STATE));
    //add_state(states, ACTION_ONE | ACTION_LEFT, states+1, ACTION_RIGHT, states);
    //add_state(states+1, ACTION_RIGHT, NULL, ACTION_LEFT, states+1);
    add_state(states, 0, NULL, ACTION_LEFT, states+1);
    //add_state(states, ACTION_LEFT, NULL, ACTION_LEFT, NULL);
    add_state(states+1, ACTION_LEFT, states+2, ACTION_LEFT, states+2);
    add_state(states+2, ACTION_ONE, states+2, ACTION_LEFT, states+3);
    add_state(states+3, ACTION_ONE, states+3, ACTION_RIGHT, states+4);
    add_state(states+4, ACTION_RIGHT, states+5, ACTION_RIGHT, states+4);
    add_state(states+5, ACTION_LEFT, states+6, ACTION_RIGHT, states+5);
    add_state(states+6, ACTION_LEFT, states+7, ACTION_ZERO, states+6);
    add_state(states+7, ACTION_LEFT, states+10, ACTION_LEFT, states+8);
    add_state(states+8, ACTION_LEFT, states+9, ACTION_LEFT, states+8);
    //add_state(states+8, ACTION_LEFT, NULL, ACTION_LEFT, NULL);
    add_state(states+9, ACTION_RIGHT, states+1, ACTION_LEFT, states+9);
    add_state(states+10, ACTION_RIGHT, NULL, ACTION_LEFT, states+10);

    memset(uc_mem.virt, 0, uc_mem.size);
    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 9*N_STATES);
    uint32_t *tape_full = alloc_uncached_uint32(&uc_mem, TAPE_SIZE+2);
    uint32_t *tape = tape_full+1;
    uint32_t *cbdata = alloc_uncached_uint32(&uc_mem, 2*N_STATES);
    uint32_t *zero = alloc_uncached_uint32(&uc_mem, 1);
    uint32_t *one = alloc_uncached_uint32(&uc_mem, 1);
    zero[0] = 0;
    one[0] = 4;
    for (int i=0; i < 7; i++) tape[i] = 4;
    //tape[0] = tape[1] = tape[TAPE_SIZE-1] = tape[TAPE_SIZE-3] = tape[TAPE_SIZE-4] = 4;
    print_tape(tape, 16);
    printf("Zero is at %08x, one is at %08x\n", MEM_BUS_ADDR(&uc_mem, zero), MEM_BUS_ADDR(&uc_mem, one));
    printf("Tape starts at %08x\n", MEM_BUS_ADDR(&uc_mem, tape));
    printf("cbdata starts at %08x\n", MEM_BUS_ADDR(&uc_mem, cbdata));

    printf("States:\n");
    uint32_t *prev;
    uint32_t zero_state, one_state;
    for (int i=0; i < N_STATES; i++) {
        printf("    %3d: 0: ", i);
        print_action(states[i].zero_action, ADDR_TO_STATE(states[i].zero_state));
        printf(", 1: ");
        print_action(states[i].one_action, ADDR_TO_STATE(states[i].one_state));
        printf("\n");
        DMA_CB *cbr = cbs+9*i;
        cbdata[2*i] = MEM_BUS_ADDR(&uc_mem, cbr+3);
        cbdata[2*i+1] = MEM_BUS_ADDR(&uc_mem, cbr+6);
        cb_mem2mem(&uc_mem, cbr, 0, 4, tape, &(cbr[1].tfr_len), cbr+1);
        cb_mem2mem(&uc_mem, cbr+1, 0, 0, cbdata+2*i+1, &(cbr[2].next_cb), cbr+2);
        cb_mem2mem(&uc_mem, cbr+2, 0, 4, cbdata+2*i, &(cbr[2].next_cb), 0);
        // If the value is zero:
        zero_state = (states[i].zero_state ? MEM_BUS_ADDR(&uc_mem, (cbs + 9*(states[i].zero_state - states))) : 0);
        prev = cbdata+2*i;
        cbdata[2*i] = zero_state;
        if (states[i].zero_action & (ACTION_ZERO | ACTION_ONE)) {
            *prev = MEM_BUS_ADDR(&uc_mem, cbr+3);
            cb_mem2mem(&uc_mem, cbr+3, 0, 4, ((states[i].zero_action & ACTION_ONE) ? one : zero), tape, 0);
            prev = &(cbr[3].next_cb);
            *prev = zero_state;
        }
        if (states[i].zero_action & ACTION_RIGHT) {
            *prev = MEM_BUS_ADDR(&uc_mem, cbr+4);
            cb_mem2mem(&uc_mem, cbr+4, 0, 4, tape, tape+TAPE_SIZE, cbr+5);
            cb_mem2mem(&uc_mem, cbr+5, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4*TAPE_SIZE, tape+1, tape, 0);
            prev = &(cbr[5].next_cb);
            *prev = zero_state;
        } else if (states[i].zero_action & ACTION_LEFT) {
            *prev = MEM_BUS_ADDR(&uc_mem, cbr+4);
            cb_mem2mem(&uc_mem, cbr+4, 0, 4, tape+TAPE_SIZE-1, tape-1, cbr+5);
            cb_mem2mem(&uc_mem, cbr+5, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4*TAPE_SIZE, tape-1, tape, 0);
            prev = &(cbr[5].next_cb);
            *prev = zero_state;
        }
        // If the value is one:
        one_state = (states[i].one_state ? MEM_BUS_ADDR(&uc_mem, (cbs + 9*(states[i].one_state - states))) : 0);
        prev = cbdata+2*i+1;
        cbdata[2*i+1] = one_state;
        if (states[i].one_action & (ACTION_ZERO | ACTION_ONE)) {
            *prev = MEM_BUS_ADDR(&uc_mem, cbr+6);
            cb_mem2mem(&uc_mem, cbr+6, 0, 4, ((states[i].one_action & ACTION_ONE) ? one : zero), tape, 0);
            prev = &(cbr[6].next_cb);
            *prev = one_state;
        }
        if (states[i].one_action & ACTION_RIGHT) {
            *prev = MEM_BUS_ADDR(&uc_mem, cbr+7);
            cb_mem2mem(&uc_mem, cbr+7, 0, 4, tape, tape+TAPE_SIZE, cbr+8);
            cb_mem2mem(&uc_mem, cbr+8, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4*TAPE_SIZE, tape+1, tape, 0);
            prev = &(cbr[8].next_cb);
            *prev = one_state;
        } else if (states[i].one_action & ACTION_LEFT) {
            *prev = MEM_BUS_ADDR(&uc_mem, cbr+7);
            cb_mem2mem(&uc_mem, cbr+7, 0, 4, tape+TAPE_SIZE-1, tape-1, cbr+8);
            cb_mem2mem(&uc_mem, cbr+8, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4*TAPE_SIZE, tape-1, tape, 0);
            prev = &(cbr[8].next_cb);
            *prev = one_state;
        }

        cbr[2].next_cb = cbdata[2*i];
        /*for (int j=0; j < 9; j++) {
            printf("    %08x: SRCE_AD: %08x, DEST_AD: %08x, NEXT_CB: %08x\n", MEM_BUS_ADDR(&uc_mem, cbr+j), cbr[j].srce_ad, cbr[j].dest_ad, cbr[j].next_cb);
        }*/
    }

    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    clock_t start_time = clock();
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    clock_t end_time = clock();
    print_tape(tape, 16);
    printf("Completed in %luus\n", end_time - start_time);
    
    done(0);
}
