#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <rpihw/rpihw.h>
#include "alu.h"
#include "lut.h"
#include "ldst.h"

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
        printf("%02x", regfile[i]);
        if ((i%8) == 7) printf("\n");
        else printf(", ");
    }
}

void print_ram(uint8_t *ram) {
    for (uint8_t i=0; i < 64; i++) {
        //if (i < 10) printf("%02x", i, extract_bits(regfile+8*i));
        //else 
        printf("%02x", ram[i]);
        if ((i%8) == 7) printf("\n");
        else printf(", ");
    }
    printf("     ");
    for (uint8_t i=0; i < 32; i++) printf(" %02x", i);
    printf("\n");
    for (uint16_t i=0; i < 512; i++) {
        if ((i % 32) == 0) printf("%04x:", i);
        printf(" %02x", ram[64+i]);
        if ((i % 32) == 31) printf("\n");
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

uint8_t parse_hex(char *buf) {
    return ((buf[0] >= 'A' ? buf[0] - 'A' + 10 : buf[0] - '0') << 4) | (buf[1] >= 'A' ? buf[1] - 'A' + 10 : buf[1] - '0');
}

int main(int argc, char **argv) {
    signal(SIGINT, interrupt);
    map_periph(&dma_regs, (void *)DMA_BASE, PAGE_SIZE);
    map_periph(&gpio_regs, (void *)GPIO_BASE, PAGE_SIZE);
    map_uncached_mem(&uc_mem, 16*PAGE_SIZE);

    gpio_mode(16, GPIO_OUT);
    
    DMA_CB *cbs = alloc_uncached_cbs(&uc_mem, 512);
    memset(cbs, 0, 256*sizeof(DMA_CB));
    uint16_t *prog = alloc_uncached_uint16(&uc_mem, 1024);
    memset(prog, 0, 1024*sizeof(uint16_t));
    int wpc = 0;
    //prog[wpc++] = 0b0000111100000001;   // ADD r16, r17
    //prog[wpc++] = 0b0010111100100000;   // MOV r18, r16
    //prog[wpc++] = 0b0001111110001001;   // ADC r24, r25
    //prog[wpc++] = 0b0001101100001000;   // SUB r16, r24
    //prog[wpc++] = 0b0000101100011001;   // SBC r17, r25
    //prog[wpc++] = 0b1110101000110101;   // LDI r19, 0xa5
    //prog[wpc++] = 0b1110100100001100;   // LDI r16, -100
    //prog[wpc++] = 0b1110011000010100;   // LDI r17, 100
    //prog[wpc++] = 0b0001011110001001;   // SUB r24, r25
    //prog[wpc++] = 0b0110100100001100;   // ORI r16, 0x9c
    //prog[wpc++] = 0b0111011000010100;   // ANDI r17, 0x64
    //prog[wpc++] = 0b1001010011001000;   // BCLR 4
    //prog[wpc++] = 0b1100111111111110;   // RJMP -2
    prog[wpc++] = 0b1110000011101000;   // LDI r30, 8
    prog[wpc++] = 0b1110100011000000;   // LDI r28, 0x80
    prog[wpc++] = 0b1110000011010000;   // LDI r29, 0x00
    prog[wpc++] = 0b0000111111111110;   // ADD r31, r30
    prog[wpc++] = 0b1001001111111001;   // ST Y+, r31
    prog[wpc++] = 0b1001010111101010;   // DEC r30
    prog[wpc++] = 0b1111011111100001;   // BRNE .-8
    if (argc == 2) {
        FILE *fptr = fopen(argv[1], "r");
        char buf[64];
        int n_bytes = 0;
        int n_read;
        do {
            n_read = fread(buf+n_bytes, 1, 64-n_bytes, fptr);
            n_bytes += n_read;
            if (buf[0] == ':') {
                uint8_t len = parse_hex(buf+1);
                uint16_t addr = (parse_hex(buf+3)<<8) | parse_hex(buf+5);
                if ((buf[7] == '0') && (buf[8] == '0')) {
                    for (uint8_t i=0; i < len; i++) {
                        ((uint8_t*)prog)[addr+i] = parse_hex(buf+9+2*i);
                    }
                }
                n_bytes -= 11 + 2*len;
                for (int i=0; i < n_bytes; i++) {
                    buf[i] = buf[i + 11 + 2*len];
                }
            } else {
                uint8_t j=0;
                for (uint8_t i=0; i < n_bytes; i++) {
                    if (j) {
                        buf[j] = buf[i];
                        j++;
                    } else if (buf[i] == ':') {
                        buf[j] = buf[i];
                        j = 1;
                    }
                }
                n_bytes = j;
            }
        } while (n_bytes || n_read);
        for (int i=0; i < 0x50; i++) {
            printf("%04x ", prog[i]);
            if ((i % 16) == 15) printf("\n");
        }
        printf("\n");
    }
    //prog[wpc++] = 0;
    uint8_t *regfile = alloc_uncached_uint8(&uc_mem, 32);
    //load_bits(0x55, regfile+7*8);
    //load_bits(0x37, regfile+(16*8));
    //load_bits(0xf3, regfile+(17*8));
    //load_bits(0xc1, regfile+(24*8));
    //load_bits(0xff, regfile+(25*8));
    regfile[16] = 0x37;
    regfile[17] = 0xf3;
    regfile[24] = 0xc1;
    regfile[25] = 0xff;
    uint8_t *ram = alloc_uncached_uint8(&uc_mem, 576);
    for (int i=96; i < 106; i++) regfile[i] = 0xaa;
#ifdef DEBUG_LOG
    uint16_t *log = alloc_uncached_uint16(&uc_mem, 256);
#endif

    // lut.c
    uint16_t *addlut = alloc_uncached_addlut(&uc_mem);
    uint8_t *lut_8to64 = alloc_uncached_8to64(&uc_mem);
    uint16_t *tmp = alloc_uncached_uint16(&uc_mem, 256);
    uint8_t *tmp8 = alloc_uncached_uint8(&uc_mem, 256);

    // alu.c
    uint8_t *rd = alloc_uncached_uint8(&uc_mem, 16);
    uint8_t *rr = alloc_uncached_uint8(&uc_mem, 16);
    uint32_t *alucon_arithmetic = alloc_uncached_uint32(&uc_mem, 8);
    uint32_t *alucon_bitwise = alloc_uncached_uint32(&uc_mem, 8);
    uint32_t *alucon_word = alloc_uncached_uint32(&uc_mem, 16);
    uint32_t *alucon_sr = alloc_uncached_uint32(&uc_mem, 16);
    uint8_t *alu_lut = alloc_uncached_uint8(&uc_mem, 12);
    alu_lut[0] = 0; alu_lut[1] = 0; alu_lut[2] = 0;
    alu_lut[3] = 1; alu_lut[4] = 0; alu_lut[5] = 1;
    alu_lut[6] = 0; alu_lut[7] = 1; alu_lut[8] = 1;
    alu_lut[9] = 1; alu_lut[10] = 1; alu_lut[11] = 1;
    uint8_t *alu_lut_sr = alloc_uncached_uint8(&uc_mem, 12);
    uint32_t *unary_lut = alloc_uncached_uint32(&uc_mem, 16);
    uint8_t *flag_lut = alloc_uncached_uint8(&uc_mem, 16);
    memset(flag_lut, 0, 16);
    flag_lut[2] = flag_lut[7] = flag_lut[11] = 1;
    flag_lut[12] = flag_lut[13] = flag_lut[15] = 1;
    uint8_t *sreg = alloc_uncached_uint8(&uc_mem, 8);
    uint8_t *carry = alloc_uncached_uint8(&uc_mem, 1);
    uint8_t *zcarry = alloc_uncached_uint8(&uc_mem, 1);
    
    uint16_t *pc = alloc_uncached_uint16(&uc_mem, 2);
    uint16_t *pc_incr = alloc_uncached_uint16(&uc_mem, 1);
    pc_incr[0] = 2;
    uint8_t *zero = alloc_uncached_uint8(&uc_mem, 1);
    zero[0] = 0;
    uint8_t *instr = alloc_uncached_uint8(&uc_mem, 16);
    instruction *instr_lut = alloc_uncached(&uc_mem, 256*sizeof(instruction));
    instruction *tmp_instr = alloc_uncached(&uc_mem, sizeof(instruction));
    uint32_t *regfile_ptrs = alloc_uncached_uint32(&uc_mem, 32);
    for (int i=0; i < 32; i++) regfile_ptrs[i] = MEM_BUS_ADDR(&uc_mem, regfile+i);
    uint32_t *prog_ptr = alloc_uncached_uint32(&uc_mem, 1);
    prog_ptr[0] = MEM_BUS_ADDR(&uc_mem, prog);

    uint32_t *pin16 = alloc_uncached_uint32(&uc_mem, 1);
    pin16[0] = 1<<16;

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

    // Load SREG
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(ram+0x3f), CC_MREF(sreg));

#ifdef DEBUG_LOG
    // Log PC
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 510, CC_MREF(log+1), CC_MREF(log));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(pc), CC_MREF(log+255));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 1, CC_MREF(ram+0x3f), cc_ofs(CC_MREF(log+255), 1));
#endif

    // Set zcarry
    CC_REF(0).unused = 1;
    cc_mem2mem(ctx, 0, 1, CC_RREF(0, unused), CC_MREF(zcarry));

    cc_mem2reg(ctx, gpio_regs, DMA_CB_SRCE_INC, 4, CC_MREF(pin16), GPIO_SET0);

    // Clear internal ALU carry bit
    CC_REF(0).unused = 0;
    DMA_CB *cbptr_jump_load1 = cc_label(ctx, "jump_load1", cc_mem2mem(ctx, 0, 1, CC_RREF(0, unused), CC_MREF(carry)));


    // FIRST LOAD
    // Subroutine for loading the carry bit
    DMA_CB *cbptr_load_carry = cc_mem2mem(ctx, 0, 1, CC_MREF(sreg), CC_MREF(carry));
    cc_mem2mem(ctx, 0, 1, CC_MREF(sreg+1), CC_MREF(zcarry));
    cc_goto(ctx, CC_CREF("jump_load2"));

    // Subroutine for storing the program counter to the stack
    DMA_CB *cbptr_save_pc = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(regfile-3), CC_MREF(tmp8));
    CC_REF(0).stride = (0xfffe)<<16;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 1, CC_MREF(pc), CC_MREF(tmp8+3));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(ram + 0x3d), cc_ofs(CC_RREF(1, stride), 2));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 2, CC_MREF(tmp8), CC_MREF(regfile-3));
    CC_REF(-1).unused = 0xfffe;
    CC_REF(-2).unused = 0;
    cc_add16(ctx, CC_MREF(addlut), CC_RREF(-1, unused), CC_MREF(ram+0x3d), CC_RREF(-2, unused), CC_MREF(tmp), CC_MREF(tmp8));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(tmp8), CC_MREF(ram+0x3d));


    // SECOND LOAD
    // Dummy block for jumping to the desired loader
    DMA_CB *cbptr_jump_load2 = cc_label(ctx, "jump_load2", cc_dummy(ctx));

    // Subroutine for loading Rr
    DMA_CB *cbptr_load_rd = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(regfile), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 16, CC_MREF(instr+9), 1);
    cc_combined_shift(ctx, CC_MREF(tmp8), 1, CC_MREF(instr), 4);
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(tmp8), CC_MREF(rr));

    // Subroutine for loading Rd
    DMA_CB *cbptr_load_d = cc_label(ctx, "load_d", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(regfile), CC_MREF(tmp8)));
    cc_combined_shift(ctx, CC_MREF(tmp8), 1, CC_MREF(instr+4), 5);
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(tmp8), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_oper"));
    
    // Subroutine for loading Rr with an immediate value
    DMA_CB *cbptr_load_k = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(instr), CC_MREF(rr));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(instr+8), CC_MREF(rr+4));

    // Subroutine for loading Rd from the upper registers only
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 16, CC_MREF(regfile+16), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 1, CC_MREF(instr+4), 4);
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(tmp8), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_oper"));

    // Subroutine for loading an immediate value for RJMP (with sign extension)
    DMA_CB *cbptr_load_jk = cc_mem2mem(ctx, 0, 1, CC_RREF(0, unused), CC_MREF(rr));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 12, CC_MREF(instr), CC_MREF(rr+1));
    CC_REF(0).stride = 0xffff;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 1, CC_MREF(rr+12), CC_MREF(rr+13));
    cc_goto(ctx, CC_CREF("load_pc"));

    // Subroutine for loading an immediate value for BRBC / BRBS (with sign extension)
    DMA_CB *cbptr_load_brk = cc_mem2mem(ctx, 0, 1, CC_RREF(0, unused), CC_MREF(rr));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7, CC_MREF(instr+3), CC_MREF(rr+1));
    CC_REF(0).stride = 0xffff;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (7<<16) | 1, CC_MREF(rr+7), CC_MREF(rr+8));
    cc_goto(ctx, CC_CREF("load_pc"));

    // Subroutine for loading the program counter
    DMA_CB *cbptr_load_pc = cc_label(ctx, "load_pc", cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(pc), CC_MREF(rd)));
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), cc_ofs(CC_MREF(pc), 1), CC_MREF(rd+8));
    cc_goto(ctx, CC_CREF("jump_oper"));

    // Subroutine for loading values for ADIW/SBIW
    DMA_CB *cbptr_load_w = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(instr), CC_MREF(rr));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(instr+6), CC_MREF(rr+4));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(regfile+24), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 2, CC_MREF(instr+4), 2);
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(tmp8), CC_MREF(rd));
    cc_convert_8to64(ctx, CC_MREF(lut_8to64), CC_MREF(tmp8+1), CC_MREF(rd+8));
    

    // OPERATION
    // Dummy block for jumping to the desired operation
    DMA_CB *cbptr_jump_oper = cc_label(ctx, "jump_oper", cc_dummy(ctx));

    // Subroutine for subtraction
    DMA_CB *cbptr_oper_alusub = cc_inv(ctx, CC_MREF(rr), CC_MREF(rr), 8);
    cc_inv(ctx, CC_MREF(carry), CC_MREF(carry), 1);
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("negate_carry"), CC_DREF("alu_end", next_cb));

    // Subroutine for addition
    DMA_CB *cbptr_oper_aluadd = cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(alu_lut_sr), cc_ofs(CC_DREF("alu_root", srce_ad), 12*32));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(alucon_arithmetic), CC_MREF(alucon_sr));
    cc_goto(ctx, CC_CREF("alu_begin"));

    // Subroutine for XOR
    DMA_CB *cbptr_oper_aluxor = cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(alu_lut_sr), cc_ofs(CC_DREF("alu_root", srce_ad), 12*32));
    cc_goto(ctx, CC_CREF("alu_load_bw"));

    // Subroutine for AND
    DMA_CB *cbptr_oper_aluand = cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(alu_lut_sr+1), cc_ofs(CC_DREF("alu_root", srce_ad), 12*32));
    cc_goto(ctx, CC_CREF("alu_load_bw"));

    // Subroutine for OR
    DMA_CB *cbptr_oper_aluor = cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(alu_lut_sr+2), cc_ofs(CC_DREF("alu_root", srce_ad), 12*32));
    cc_goto(ctx, CC_CREF("alu_load_bw"));

    // Internal subroutine for loading alucon
    cc_label(ctx, "alu_load_bw", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(alucon_bitwise), CC_MREF(alucon_sr)));
    cc_goto(ctx, CC_CREF("alu_begin"));

    // Subroutine for BRBC / BRBS instructions
    DMA_CB *cbptr_oper_brb = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(sreg), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 1, CC_MREF(instr), 3);
    CC_REF(0).stride = (4<<16) | 0xfffc;
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 4, CC_CREF("finish"), CC_MREF(tmp8+4));
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("oper_aluword"), CC_MREF(tmp8+8));
    cc_mem2mem(ctx, 0, 1, CC_MREF(tmp8), CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7<<16, CC_MREF(tmp8+8), CC_MREF(tmp8+4));
    cc_mem2mem(ctx, 0, 1, CC_MREF(instr+10), CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_TDMODE | DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 7<<16, CC_MREF(tmp8+8), CC_MREF(tmp8+4));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8+4), CC_RREF(1, next_cb));
    cc_dummy(ctx);


    // Subroutine for 16 bit subtraction
    DMA_CB *cbptr_oper_aluwsub = cc_inv(ctx, CC_MREF(rr), CC_MREF(rr), 16);
    cc_inv(ctx, CC_MREF(carry), CC_MREF(carry), 1);
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("negate_carry"), CC_DREF("alu_end", next_cb));

    // Subroutine for performing a 16 bit addition
    DMA_CB *cbptr_oper_aluword = cc_label(ctx, "oper_aluword", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 64, CC_MREF(alucon_word), 
                                          CC_MREF(alucon_sr)));
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(rd+15), cc_ofs(CC_DREF("alu_root", dest_ad), 14*32));
    
    // Internal ALU subroutine
    cc_label(ctx, "alu_begin", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 16, CC_MREF(flag_lut), CC_MREF(tmp8)));
    cc_combined_shift(ctx, CC_MREF(tmp8), 8, CC_MREF(rd+7), 1);
    cc_combined_shift(ctx, CC_MREF(tmp8), 4, CC_MREF(rr+7), 1);
    DMA_CB *cbptr_alu_root = cc_label(ctx, "alu_root", cc_alu8(ctx, CC_MREF(alu_lut), CC_MREF(rd), CC_MREF(rr), CC_MREF(carry), 
                                     CC_MREF(alucon_sr), CC_MREF(alu_lut_sr), CC_MREF(sreg)));
    cc_combined_shift(ctx, CC_MREF(tmp8), 2, CC_MREF(rd+7), 1);
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(rd+7), cc_ofs(CC_DREF("alu_root", dest_ad), 14*32));
    cc_label(ctx, "alu_end", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(tmp8), CC_MREF(sreg+3)));
    cc_goto(ctx, CC_CREF("jump_store"));
    populate_alucon(&uc_mem, alucon_arithmetic, cbptr_alu_root, ALUCON_CZNVSH);
    populate_alucon(&uc_mem, alucon_bitwise, cbptr_alu_root, ALUCON_ZNVS);
    populate_alucon(&uc_mem, alucon_word, cbptr_alu_root, ALUCON_CZNVS);

    // Internal subroutine for negating the carry bit and updating Z
    cc_label(ctx, "negate_carry", cc_inv(ctx, CC_MREF(sreg), CC_MREF(sreg), 1));
    cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("jump_store"), CC_DREF("alu_end", next_cb));
    cc_mem2mem(ctx, 0, 1, CC_MREF(sreg+1), CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, 0, 0, CC_MREF(zcarry), CC_MREF(sreg+1));
    cc_goto(ctx, CC_CREF("jump_store"));

    // Subroutine for moving Rr to Rd
    DMA_CB *cbptr_oper_mov = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(rr), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_store"));

    // Subroutine for manipulating SREG
    DMA_CB *cbptr_oper_bitwrite_sreg = cc_imm2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_CREF("store_sreg"), CC_DREF("jump_store", next_cb));

    // Subroutine for writing a particular bit in Rd with the given value
    DMA_CB *cbptr_oper_bitwrite = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(rd), CC_MREF(tmp8+7));
    CC_REF(0).stride = (31<<16);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 1, CC_MREF(rr), CC_RREF(1, tfr_len));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (13<<16), CC_MREF(tmp8+1), CC_MREF(tmp8));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (12<<16), CC_MREF(tmp8+2), CC_MREF(tmp8));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (10<<16), CC_MREF(tmp8+4), CC_MREF(tmp8));
    cc_mem2mem(ctx, 0, 1, CC_MREF(rr+7), CC_MREF(tmp8+7));
    CC_REF(0).stride = (31<<16);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (2<<16) | 1, CC_MREF(rr), CC_RREF(1, tfr_len));
    CC_REF(0).stride = CC_REF(1).stride = CC_REF(2).stride = 0xfffefffe;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (13<<16), CC_MREF(tmp8+13), CC_MREF(tmp8+14));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (12<<16), CC_MREF(tmp8+12), CC_MREF(tmp8+14));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (10<<16), CC_MREF(tmp8+10), CC_MREF(tmp8+14));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(tmp8+7), CC_MREF(rd));
    cc_goto(ctx, CC_CREF("jump_store"));

    // Subroutine for unary ALU
    DMA_CB *cbptr_oper_unary = cc_unary_alu(ctx, CC_MREF(unary_lut), CC_MREF(lut_8to64), CC_MREF(tmp8), CC_MREF(instr),
                                            CC_MREF(rd), CC_MREF(rr), CC_MREF(sreg), cbptr_oper_aluadd, cbptr_oper_alusub, cbptr_oper_bitwrite_sreg);

    // Subroutine for LD and ST
    DMA_CB *cbptr_oper_ldst = cc_ldst(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(regfile), CC_MREF(instr));

    // STORE
    // Dummy block for jumping to the desired operation
    DMA_CB *cbptr_jump_store = cc_label(ctx, "jump_store", cc_dummy(ctx));

    // Subroutine for storing Rd
    DMA_CB *cbptr_store_d = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 128, CC_MREF(regfile_ptrs), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 4, CC_MREF(instr+4), 5);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8), CC_RREF(11, dest_ad));
    cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(rd), CC_MREF(regfile));
    cc_goto(ctx, CC_CREF("finish"));

    // Subroutine for storing Rd (upper registers only)
    DMA_CB *cbptr_store_ud = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 128, CC_MREF(regfile_ptrs+16), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 4, CC_MREF(instr+4), 4);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8), CC_RREF(11, dest_ad));
    cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(rd), CC_MREF(regfile));
    cc_goto(ctx, CC_CREF("finish"));

    // Subroutine for storing the program counter
    DMA_CB *cbptr_store_pc = cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(rd), CC_MREF(pc));
    cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(rd+8), cc_ofs(CC_MREF(pc), 1));
    cc_goto(ctx, CC_CREF("finish_no_sreg"));

    // Subroutine for storing Rd (register pairs)
    DMA_CB *cbptr_store_w = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(regfile_ptrs+24), CC_MREF(tmp8));
    cc_combined_shift(ctx, CC_MREF(tmp8), 8, CC_MREF(instr+4), 2);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8), CC_RREF(12, dest_ad));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 4, CC_MREF(tmp8+4), CC_RREF(23, dest_ad));
    cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(rd), CC_MREF(regfile));
    cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(rd+8), CC_MREF(regfile));
    cc_goto(ctx, CC_CREF("finish"));

    // Subroutine for storing SREG
    cc_label(ctx, "store_sreg", cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 8, CC_MREF(rd), CC_MREF(sreg)));
    cc_goto(ctx, CC_CREF("finish"));

    // Subroutine for storing to RAM (OUT)
    DMA_CB *cbptr_store_out = cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 32, CC_MREF(regfile), CC_MREF(tmp));
    cc_combined_shift(ctx, CC_MREF(tmp), 1, CC_MREF(instr+4), 5);
    cc_mem2mem(ctx, 0, 1, CC_MREF(regfile+31), cc_ofs(CC_MREF(tmp), 1));
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 128, CC_MREF(addlut), CC_MREF(tmp+1));
    cc_combined_shift(ctx, CC_MREF(tmp+1), 32, CC_MREF(instr+9), 2);
    cc_combined_shift(ctx, CC_MREF(tmp+1), 2, CC_MREF(instr), 4);
    cc_mem2mem(ctx, 0, 1, CC_MREF(tmp+1), cc_ofs(CC_RREF(1, stride), 2));
    CC_REF(0).stride = 0xfffe;
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC | DMA_TDMODE, (1<<16) | 1, cc_ofs(CC_MREF(tmp), 1), CC_MREF(regfile+31));


    // INCREMENT COUNTER
    DMA_CB *cbptr_finish = cc_label(ctx, "finish", cc_convert_64to8(ctx, CC_MREF(addlut), CC_MREF(tmp), CC_MREF(sreg), CC_MREF(ram+0x3f)));
    cc_label(ctx, "finish_no_sreg", cc_add16(ctx, CC_MREF(addlut), CC_MREF(pc), CC_MREF(pc_incr), CC_MREF(zero), CC_MREF(tmp), 
                                                            CC_MREF(tmp8)));
    cc_mem2reg(ctx, gpio_regs, DMA_CB_SRCE_INC, 4, CC_MREF(pin16), GPIO_CLR0);
    cc_mem2mem(ctx, DMA_CB_SRCE_INC | DMA_CB_DEST_INC, 2, CC_MREF(tmp8), CC_MREF(pc));
    cc_goto(ctx, CC_CREF("begin"));

    printf("Total number of CBs: %d, total memory %08x\n", ctx->n_cbs, uc_mem.len);
    int n_cbs = ctx->n_cbs;
    cc_clean(NULL, ctx);
    
    define_instruction(&uc_mem, instr_lut, 0b00000000, 0b00000000, NULL, NULL, NULL, NULL);
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00000100, cbptr_load_carry, cbptr_load_rd, cbptr_oper_alusub, cbptr_finish);       // CPC
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00001000, cbptr_load_carry, cbptr_load_rd, cbptr_oper_alusub, cbptr_store_d);      // SBC
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00001100, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_aluadd, cbptr_store_d);      // ADD
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00010100, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_alusub, cbptr_finish);       // CP
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00011000, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_alusub, cbptr_store_d);      // SUB
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00011100, cbptr_load_carry, cbptr_load_rd, cbptr_oper_aluadd, cbptr_store_d);      // ADC
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00100000, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_aluand, cbptr_store_d);      // AND
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00100100, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_aluxor, cbptr_store_d);      // EOR
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00101000, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_aluor, cbptr_store_d);       // OR
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b00101100, cbptr_jump_load2, cbptr_load_rd, cbptr_oper_mov, cbptr_store_d);         // MOV
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b00110000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_alusub, cbptr_finish);        // CPI
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b01000000, cbptr_load_carry, cbptr_load_k, cbptr_oper_alusub, cbptr_store_ud);      // SBCI
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b01010000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_alusub, cbptr_store_ud);      // SUBI
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b01100000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_aluor, cbptr_store_ud);       // ORI
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b01110000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_aluand, cbptr_store_ud);      // ANDI
    define_instruction(&uc_mem, instr_lut, 0b11111100, 0b10010000, cbptr_jump_load2, cbptr_jump_oper, cbptr_oper_ldst, cbptr_finish);       // LD/ST
    define_instruction(&uc_mem, instr_lut, 0b11111110, 0b10010100, cbptr_jump_load2, cbptr_load_d, cbptr_oper_unary, cbptr_store_d);
    define_instruction(&uc_mem, instr_lut, 0b11111111, 0b10010110, cbptr_jump_load2, cbptr_load_w, cbptr_oper_aluword, cbptr_store_w);      // ADIW
    define_instruction(&uc_mem, instr_lut, 0b11111111, 0b10010111, cbptr_jump_load2, cbptr_load_w, cbptr_oper_aluwsub, cbptr_store_w);      // SBIW
    define_instruction(&uc_mem, instr_lut, 0b11111000, 0b10111000, cbptr_jump_load2, cbptr_jump_oper, cbptr_jump_store, cbptr_store_out);   // OUT
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b11000000, cbptr_jump_load2, cbptr_load_jk, cbptr_oper_aluword, cbptr_store_pc);    // RJMP
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b11010000, cbptr_save_pc, cbptr_load_jk, cbptr_oper_aluword, cbptr_store_pc);       // RCALL
    define_instruction(&uc_mem, instr_lut, 0b11110000, 0b11100000, cbptr_jump_load2, cbptr_load_k, cbptr_oper_mov, cbptr_store_ud);         // LDI
    define_instruction(&uc_mem, instr_lut, 0b11111000, 0b11110000, cbptr_jump_load2, cbptr_load_brk, cbptr_oper_brb, cbptr_store_pc);

    print_regfile(regfile);
    enable_dma(DMA_CHAN);
    start_dma(&uc_mem, DMA_CHAN, cbs, 0);
    while (*REG32(dma_regs, DMA_REG(DMA_CHAN, DMA_CONBLK_AD)));

    for (int i=0; i < 16; i++) printf("%d", instr[i]);
    printf("\n");
    printf("rd: %02x%02x, rr: %02x%02x, carry: %x\n", extract_bits(rd+8), extract_bits(rd), extract_bits(rr+8), extract_bits(rr), carry[0]);
    printf("pc: %04x, sreg: %02x (ram %02x)\n", pc[0], extract_bits(sreg), ram[0x3f]);
    printf("tmp: %04x %04x %04x\n", tmp[0], tmp[1], tmp[2]);
    print_regfile(regfile);
    print_ram(ram);

#ifdef DEBUG_LOG
    for (int i=0; i < 256; i++) {
        printf("%04x ", log[i]);
        if ((i % 16) == 15) printf("\n");
    }
#endif

    done(0);
}
