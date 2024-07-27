#include <stdint.h>
#include <rpihw/rpihw.h>

#ifndef DMA_CHAIN_H
#define DMA_CHAIN_H

#define CC_REF(i) ((ctx)->start_cb[(ctx)->n_cbs + (i)])
// Used to generate a DMA_MEM_REF to a control block
#define CC_CREF(name) new_ref(name, NULL, 0)
// Used to generate a DMA_MEM_REF to a member of a control block
#define CC_DREF(name, member) new_ref(name, NULL, (uint32_t)(&(((DMA_CB*)0)->member)))
// Used to generate a DMA_MEM_REF to a particular memory location
#define CC_MREF(ptr) new_ref(NULL, ptr, 0)
// Used to generate a DMA_MEM_REF to a member of a known control block
#define CC_RREF(ofs, member) new_ref(NULL, &((ctx->start_cb)[ctx->n_cbs+(ofs)].member), 0)

#define CC_LABEL(name, cb) cc_label(ctx, name, cb)

typedef struct DMA_MEM_REF_S {
    char *name;
    void *ptr;
    uint32_t offset;
} DMA_MEM_REF;

typedef struct DMA_MEM_LINK_S {
    char *value_name;
    void *value_ptr;
    uint32_t value_offset;
    char *dest_name;
    void *dest_ptr;
    uint32_t dest_offset;
} DMA_MEM_LINK;

typedef struct DMA_MEM_LABEL_S {
    char *name;
    DMA_CB *ptr;
} DMA_MEM_LABEL;

typedef struct {
    DMA_MEM_MAP *mp;
    DMA_CB *start_cb;
    uint32_t n_cbs;
    uint8_t ret;
    int n_labels;
    int n_links;
    struct DMA_MEM_LABEL_S *labels;
    struct DMA_MEM_LINK_S *links;
} DMA_CTX;

DMA_MEM_REF new_ref(char *name, void *ptr, uint32_t offset);
void cc_link(DMA_CTX *ctx, DMA_MEM_REF value, DMA_MEM_REF dest);
void cc_label(DMA_CTX *ctx, char *name, DMA_CB *cb);

DMA_CB *cc_imm2mem(DMA_CTX *ctx, uint32_t ti, uint32_t tfr_len, DMA_MEM_REF srce, DMA_MEM_REF dest);
DMA_CB *cc_mem2mem(DMA_CTX *ctx, uint32_t ti, uint32_t tfr_len, DMA_MEM_REF srce, DMA_MEM_REF dest);

DMA_CTX *init_ctx(DMA_CTX *pctx);
void cc_goto(DMA_CTX *ctx, DMA_MEM_REF target);
void cc_ret(DMA_CTX *ctx);
DMA_CB *cc_clean(DMA_CTX *pctx, DMA_CTX *ctx);

DMA_MEM_REF cc_ofs(DMA_MEM_REF src, uint32_t n);

#endif
