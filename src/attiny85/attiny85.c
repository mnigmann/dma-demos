#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <rpihw/rpihw.h>
#include "alu.h"
#include "lut.h"

#define DMA_CHAN 5

DMA_MEM_MAP uc_mem;

typedef struct {
    uint32_t load1;
    uint32_t load2;
    uint32_t operator;
    uint32_t store;
} instruction;

void done(int sig) {
    stop_dma(DMA_CHAN);
    unmap_uncached_mem(&uc_mem);
    terminate(0);
    exit(0);
}

void interrupt(int sig) {
    printf("Interrupted on %08x\n", *REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));
    stop_dma(DMA_CHAN);
}

void define_instruction(DMA_MEM_MAP *mp, instruction *lut, uint8_t mask, uint8_t opcode, DMA_CB *load1, DMA_CB *load2, DMA_CB *oper, DMA_CB *store) {
    for (int i=0; i < 256; i++) {
        if ((i & mask) == opcode) {
            lut[i].load1 = (load1 ? MEM_BUS_ADDR(mp, load1) : 0);
            lut[i].load2 = (load2 ? MEM_BUS_ADDR(mp, load2) : 0);
            lut[i].operator = (oper ? MEM_BUS_ADDR(mp, oper) : 0);
            lut[i].store = (store ? MEM_BUS_ADDR(mp, store) : 0);
        }
    }
}

void print_regfile(uint8_t *regfile) {
    for (uint8_t i=0; i < 32; i++) {
        //if (i < 10) printf("%02x", i, extract_bits(regfile+8*i));
        //else 
        printf("%02x", extract_bits(regfile+8*i));
        if ((i%8) == 7) printf("\n");
        else printf(", ");
    }
}

void find_causes(DMA_CB *cbs, int n_cbs, uint32_t target) {
    // Determine which CBs could have cause a change to the given target address
    DMA_CB cb;
    printf("searching for %08x\n", target);
    for (int i=0; i < n_cbs; i++) {
        cb = cbs[i];
        if (cb.ti & DMA_TDMODE) {
            uint32_t start = cb.dest_ad;
            uint16_t ylen = cb.tfr_len>>16;
            uint16_t xlen = cb.tfr_len & 0xffff;
            int16_t dstride = cb.stride>>16;
            int step = xlen + dstride;
            //printf("CB %d has 2D mode, xlen %d, ylen %d, dstride %d, step %d\n", i, xlen, ylen, dstride, step);
            for (int j=0; j < ylen; j++) {
                if ((target >= start + step*j) && (target < start + step*j + xlen)) {
                    printf("CB %d could have changed %08x\n", i, target);
                    break;
                }
            }
        } else {
            if ((target >= cb.dest_ad) && (target < cb.dest_ad + cb.tfr_len)) {
                printf("CB %d could have changed %08x\n", i, target);
            }
        }
    }
}

int main(void) {
    signal(SIGINT, interrupt);
    map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE);
    map_uncached_mem(&uc_mem, 16*PAGE_SIZE);
    
    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 256);
    memset(cbs, 0, 256*sizeof(DMA_CB));
    uint16_t *prog = alloc_uncached_uint16(&uc_mem, 32);
    int wpc = 0;
    //prog[wpc++] = 0b0000111100000001;   // ADD r16, r17
    //prog[wpc++] = 0b0010111100100000;   // MOV r18, r16
    //prog[wpc++] = 0b0001111110001001;   // ADC r24, r25
    //prog[wpc++] = 0b0001101100001000;   // SUB r16, r24
    //prog[wpc++] = 0b0000101100011001;   // SBC r17, r25
    //prog[wpc++] = 0b1110101000110101;   // LDI r19, 0xa5
    prog[wpc++] = 0b1110100100001100;   // LDI r16, -100
    prog[wpc++] = 0b1110011000010100;   // LDI r17, 100
    prog[wpc++] = 0b0001101100000001;   // SUB r16, r17
    prog[wpc++] = 0;
    uint8_t *regfile = alloc_uncached_uint8(&uc_mem, 32*8);
    load_bits(0xaa, regfile);
    load_bits(0x37, regfile+(16*8));
    load_bits(0xf3, regfile+(17*8));
    load_bits(0x37, regfile+(24*8));
    load_bits(0xf3, regfile+(25*8));

    // lut.c
    uint16_t *addlut = alloc_uncached_addlut(&uc_mem);
    uint8_t *lut_8to64 = alloc_uncached_8to64(&uc_mem);
    uint16_t *tmp = alloc_uncached_uint16(&uc_mem, 256);
    uint8_t *tmp8 = alloc_uncached_uint8(&uc_mem, 256);

    // alu.c
    uint8_t *rd = alloc_uncached_uint8(&uc_mem, 8);
    uint8_t *rr = alloc_uncached_uint8(&uc_mem, 8);
    uint32_t *alucon_sr = alloc_uncached_uint32(&uc_mem, 8);
    uint8_t *alu_lut = alloc_uncached_uint8(&uc_mem, 12);
    alu_lut[0] = 0; alu_lut[1] = 0; alu_lut[2] = 0;
    alu_lut[3] = 1; alu_lut[4] = 0; alu_lut[5] = 1;
    alu_lut[6] = 0; alu_lut[7] = 1; alu_lut[8] = 1;
    alu_lut[9] = 1; alu_lut[10] = 1; alu_lut[11] = 1;
    uint8_t *alu_lut_sr = alloc_uncached_uint8(&uc_mem, 12);
    uint8_t *flag_lut = alloc_uncached_uint8(&uc_mem, 16);
    memset(flag_lut, 0, 16);
    flag_lut[2] = flag_lut[7] = flag_lut[11] = 1;
    flag_lut[12] = flag_lut[13] = flag_lut[15] = 1;
    uint8_t *sreg = alloc_uncached_uint8(&uc_mem, 8);
    uint8_t *carry = alloc_uncached_uint8(&uc_mem, 1);
    
    uint16_t *pc = alloc_uncached_uint16(&uc_mem, 2);
    uint16_t *pc_incr = alloc_uncached_uint16(&uc_mem, 1);
    pc_incr[0] = 2;
    uint8_t *zero = alloc_uncached_uint8(&uc_mem, 1);
    zero[0] = 0;
    uint8_t *instr = alloc_uncached_uint8(&uc_mem, 16);
    instruction *instr_lut = alloc_uncached(&uc_mem, 256*sizeof(instruction));
    instruction *tmp_instr = alloc_uncached(&uc_mem, sizeof(instruction));
    uint32_t *regfile_ptrs = alloc_uncached_uint32(&uc_mem, 32);
    for (int i=0; i < 32; i++) regfile_ptrs[i] = MEM_BUS_ADDR(&uc_mem, regfile+8*i);
    uint32_t *prog_ptr = alloc_uncached_uint32(&uc_mem, 32);
    prog_ptr[0] = MEM_BUS_ADDR(&uc_mem, prog);

    DMA_CTX *ctx = init_ctx(NULL);
    ctx->mp = &uc_mem;
    ctx->start_cb = cbs;
    cc_label(ctx, "begin", cc_add_pc(ctx, CC_MREF(addlut), CC_MREF(prog_ptr), CC_MREF(pc), CC_MREF(tmp), CC_DREF("load_instr", srce_ad)));
    DMA_CB *cbptr_load_instr = &CC_REF(0);
    cc_label(ctx, "load_instr", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(prog), CC_DREF("load_instr", unused)));
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_DREF("load_instr", unused), CC_MREF(instr));
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), cc_ofs(CC_DREF("load_instr", unused), 1), CC_MREF(instr+8));
    // Set the jump points
    cc_lut(ctx, CC_MREF(instr_lut), cc_ofs(CC_DREF("load_instr", unused), 1), CC_MREF(tmp_instr), sizeof(instruction), sizeof(instruction));
    uint32_t jump_loader = ctx->n_cbs;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(&(tmp_instr->load1)), CC_DREF("jump_load1", next_cb));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(&(tmp_instr->load2)), CC_DREF("jump_load2", next_cb));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(&(tmp_instr->operator)), CC_DREF("jump_oper", next_cb));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(&(tmp_instr->store)), CC_DREF("jump_store", next_cb));
    //cc_ret(ctx);

    // Clear internal ALU carry bit
    CC_REF(0).unused = 0;
    DMA_CB *cbptr_jump_load1 = cc_label(ctx, "jump_load1", cc_mem2mem(ctx, 0, 1, CC_RREF(0, unused), CC_MREF(carry)));


    // FIRST LOAD
    // Subroutine for loading the carry bit
    DMA_CB *cbptr_load_carry = cc_mem2mem(ctx, 0, 1, CC_MREF(sreg), CC_MREF(carry));
    cc_goto(ctx, CC_CREF("jump_load2"));


    // SECOND LOAD
    // Dummy block for jumping to the desired loader
    DMA_CB *cbptr_jump_load2 = cc_label(ctx, "jump_load2", cc_dummy(ctx));

    // Subroutine for loading Rr
    DMA_CB *cbptr_load_rd = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32*8, CC_MREF(regfile), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 16*8, CC_MREF(instr+9), 1);
    cc_combined_shift(ctx, CC_MREF(tmp8), 8, CC_MREF(instr), 4);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(tmp8), CC_MREF(rr));

    // Subroutine for loading Rd
    DMA_CB *cbptr_load_d = cc_label(ctx, "load_d", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32*8, CC_MREF(regfile), CC_MREF(tmp8)));
    cc_combined_shift(ctx, CC_MREF(tmp8), 8, CC_MREF(instr+4), 5);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(tmp8), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_oper"));
    
    // Subroutine for loading Rr with an immediate value
    DMA_CB *cbptr_load_k = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(instr), CC_MREF(rr));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(instr+8), CC_MREF(rr+4));

    // Subroutine for loading Rd from the upper registers only
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 16*8, CC_MREF(regfile+16*8), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 8, CC_MREF(instr+4), 4);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(tmp8), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_oper"));


    // OPERATION
    // Dummy block for jumping to the desired operation
    DMA_CB *cbptr_jump_oper = cc_label(ctx, "jump_oper", cc_dummy(ctx));

    // Subroutine for subtraction
    DMA_CB *cbptr_oper_alusub = cc_inv(ctx, CC_MREF(rr), CC_MREF(rr), 8);
    cc_inv(ctx, CC_MREF(carry), CC_MREF(carry), 1);
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("negate_carry"), CC_DREF("alu_end", next_cb));
    cc_goto(ctx, CC_CREF("alu_begin"));

    // Subroutine for addition
    DMA_CB *cbptr_oper_aluadd = cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("jump_store"), CC_DREF("alu_end", next_cb));
    cc_goto(ctx, CC_CREF("alu_begin"));

    // Internal ALU subroutine
    uint32_t *alucon = alloc_uncached_alucon(&uc_mem, &CC_REF(6), ALUCON_CZNVSH);
    DMA_CB *cbptr_oper_alu = cc_label(ctx, "alu_begin", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(alucon), CC_MREF(alucon_sr)));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 16, CC_MREF(flag_lut), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 8, CC_MREF(rd+7), 1);
    cc_combined_shift(ctx, CC_MREF(tmp8), 4, CC_MREF(rr+7), 1);
    cc_alu8(ctx, CC_MREF(alu_lut), CC_MREF(rd), CC_MREF(rr), CC_MREF(carry), CC_MREF(alucon_sr), CC_MREF(alu_lut_sr), CC_MREF(sreg));
    cc_combined_shift(ctx, CC_MREF(tmp8), 2, CC_MREF(rd+7), 1);
    cc_label(ctx, "alu_end", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(tmp8), CC_MREF(sreg+3)));
    cc_goto(ctx, CC_CREF("jump_store"));

    // Internal subroutine for negating the carry bit
    cc_label(ctx, "negate_carry", cc_inv(ctx, CC_MREF(sreg), CC_MREF(sreg), 1));
    cc_goto(ctx, CC_CREF("jump_store"));

    // Subroutine for moving Rr to Rd
    DMA_CB *cbptr_oper_mov = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(rr), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_store"));


    // STORE
    // Dummy block for jumping to the desired operation
    DMA_CB *cbptr_jump_store = cc_label(ctx, "jump_store", cc_dummy(ctx));

    // Subroutine for storing Rd
    DMA_CB *cbptr_store_d = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 128, CC_MREF(regfile_ptrs), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 4, CC_MREF(instr+4), 5);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8), CC_RREF(1, dest_ad));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(rd), CC_MREF(regfile));
    cc_goto(ctx, CC_CREF("finish"));

    // Subroutine for storing Rd (upper registers only)
    DMA_CB *cbptr_store_ud = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 128, CC_MREF(regfile_ptrs+16), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 4, CC_MREF(instr+4), 4);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8), CC_RREF(1, dest_ad));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(rd), CC_MREF(regfile));
    cc_goto(ctx, CC_CREF("finish"));

    // INCREMENT COUNTER
    cc_label(ctx, "finish", cc_add16(ctx, CC_MREF(addlut), CC_MREF(pc), CC_MREF(pc_incr), CC_MREF(zero), CC_MREF(tmp), CC_MREF(pc)));
    cc_goto(ctx, CC_CREF("begin"));

    printf("Total number of CBs: %d, total memory %08x\n", ctx->n_cbs, uc_mem.len);
    int n_cbs = ctx->n_cbs;
    cc_clean(NULL, ctx);
    
    define_instruction(&uc_mem, instr_lut, 0b00000000, 0b00000000, NULL, NULL, NULL, NULL);
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00001000, cbptr_load_carry, cbptr_load_rd, cbptr_oper_alusub, cbptr_store_d);      // SBC
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00001100, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_aluadd, cbptr_store_d);      // ADD
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00011000, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_alusub, cbptr_store_d);      // SUB
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00011100, cbptr_load_carry, cbptr_load_rd, cbptr_oper_aluadd, cbptr_store_d);      // ADC
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00101100, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_mov, cbptr_store_d);         // MOV
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b01000000, cbptr_load_carry, cbptr_load_k, cbptr_oper_alusub, cbptr_store_ud);      // SBCI
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b01010000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_alusub, cbptr_store_ud);      // SUBI
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b11100000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_mov, cbptr_store_ud);         // LDI

    print_regfile(regfile);
    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));

    for (int i=0; i < 16; i++) printf("%d", instr[i]);
    printf("\n");
    printf("rd: %02x, rr: %02x, carry: %x\n", extract_bits(rd), extract_bits(rr), carry[0]);
    printf("pc: %04x, sreg: %02x\n", pc[0], extract_bits(sreg));
    print_regfile(regfile);

    done(0);
}
