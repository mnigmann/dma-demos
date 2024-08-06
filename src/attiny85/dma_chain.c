#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <rpihw/rpihw.h>
#include <string.h>
#include "dma_chain.h"

DMA_CTX *init_ctx(DMA_CTX *pctx) {
    DMA_CTX *ctx = malloc(sizeof(DMA_CTX));
    if (pctx) {
        cc_goto(pctx, CC_MREF(pctx->start_cb + pctx->n_cbs));
        ctx->mp = pctx->mp;
        ctx->start_cb = pctx->start_cb + pctx->n_cbs;
    }
    ctx->n_cbs = ctx->n_labels = ctx->n_links = 0;
    ctx->s_labels = ctx->s_links = 16;
    ctx->labels = malloc(16*sizeof(DMA_MEM_LABEL));
    ctx->links = malloc(16*sizeof(DMA_MEM_LINK));
    //printf("allocated %p, %p, and %p\n", ctx->labels, ctx->links, ctx);
    return ctx;
}

DMA_CB *cc_mem2mem(DMA_CTX *ctx, uint32_t ti, uint32_t tfr_len, DMA_MEM_REF srce, DMA_MEM_REF dest) {
    DMA_CB *cb = ctx->start_cb + ctx->n_cbs;
    cc_goto(ctx, CC_MREF(cb));
    cb->ti = ti;
    cb->tfr_len = tfr_len;
    if (srce.ptr) cb->srce_ad = MEM_BUS_ADDR(ctx->mp, srce.ptr + srce.offset);
    else cc_link(ctx, srce, CC_MREF(&(cb->srce_ad)));
    if (dest.ptr) cb->dest_ad = MEM_BUS_ADDR(ctx->mp, dest.ptr + dest.offset);
    else cc_link(ctx, dest, CC_MREF(&(cb->dest_ad)));
    cb->next_cb = 0;
    cc_link(ctx, CC_CREF("__end__"), CC_MREF(&(cb->next_cb)));
    ctx->n_cbs++;
    return cb;
}

DMA_CB *cc_mem2reg(DMA_CTX *ctx, MEM_MAP reg, uint32_t ti, uint32_t tfr_len, DMA_MEM_REF srce, uint32_t dest) {
    DMA_CB *cb = ctx->start_cb + ctx->n_cbs;
    cc_goto(ctx, CC_MREF(cb));
    cb->ti = ti;
    cb->tfr_len = tfr_len;
    if (srce.ptr) cb->srce_ad = MEM_BUS_ADDR(ctx->mp, srce.ptr + srce.offset);
    else cc_link(ctx, srce, CC_MREF(&(cb->srce_ad)));
    cb->dest_ad = REG_BUS_ADDR(reg, dest);
    cb->next_cb = 0;
    cc_link(ctx, CC_CREF("__end__"), CC_MREF(&(cb->next_cb)));
    ctx->n_cbs++;
    return cb;
}

DMA_CB *cc_imm2mem(DMA_CTX *ctx, uint32_t ti, uint32_t tfr_len, DMA_MEM_REF srce, DMA_MEM_REF dest) {
    DMA_CB *cb = ctx->start_cb + ctx->n_cbs;
    cc_goto(ctx, CC_MREF(cb));
    cb->ti = ti;
    cb->tfr_len = tfr_len;
    cb->srce_ad = MEM_BUS_ADDR(ctx->mp, &(cb->unused));
    if (srce.ptr) cb->unused = MEM_BUS_ADDR(ctx->mp, srce.ptr + srce.offset);
    else cc_link(ctx, srce, CC_MREF(&(cb->unused)));
    if (dest.ptr) cb->dest_ad = MEM_BUS_ADDR(ctx->mp, dest.ptr + dest.offset);
    else cc_link(ctx, dest, CC_MREF(&(cb->dest_ad)));
    cb->next_cb = 0;
    cc_link(ctx, CC_CREF("__end__"), CC_MREF(&(cb->next_cb)));
    ctx->n_cbs++;
    return cb;
}

DMA_CB *cc_dummy(DMA_CTX *ctx) {
    DMA_CB *cb = ctx->start_cb + ctx->n_cbs;
    cc_goto(ctx, CC_MREF(cb));
    cb->ti = 0;
    cb->tfr_len = 0;
    cb->next_cb = 0;
    cc_link(ctx, CC_CREF("__end__"), CC_MREF(&(cb->next_cb)));
    ctx->n_cbs++;
    return cb;
}

void cc_link(DMA_CTX *ctx, DMA_MEM_REF value, DMA_MEM_REF dest) {
    if (ctx->n_links >= ctx->s_links) {
        ctx->s_links += 16;
        ctx->links = realloc(ctx->links, ctx->s_links * sizeof(DMA_MEM_LINK));
    }
    DMA_MEM_LINK *l = ctx->links + ctx->n_links;
    l->value_name = value.name;
    l->value_ptr = value.ptr;
    l->value_offset = value.offset;
    l->dest_name = dest.name;
    l->dest_ptr = dest.ptr;
    l->dest_offset = dest.offset;
    ctx->n_links++;
}

DMA_CB *cc_label(DMA_CTX *ctx, char *name, DMA_CB *ptr) {
    if (ctx->n_labels >= ctx->s_labels) {
        ctx->s_labels += 16;
        ctx->labels = realloc(ctx->labels, ctx->s_labels * sizeof(DMA_MEM_LABEL));
    }
    DMA_MEM_LABEL *l = ctx->labels + ctx->n_labels;
    l->name = name;
    l->ptr = ptr;
    ctx->n_labels++;
    return ptr;
}

void cc_goto(DMA_CTX *ctx, DMA_MEM_REF target) {
    if (target.ptr) {
        // If the address of the next CB is known, substitute the address where linked
        int j = 0;
        for (int i=0; i < ctx->n_links; i++) {
            //printf("goto: link from %s\n", ctx->links[i].value_name);
            if ((ctx->links[i].value_name) && (strcmp(ctx->links[i].value_name, "__end__") == 0) && (ctx->links[i].dest_ptr)) {
                ((uint32_t*)(ctx->links[i].dest_ptr + ctx->links[i].dest_offset))[0] = MEM_BUS_ADDR(ctx->mp, target.ptr);
            } else {
                if (i != j) memcpy(ctx->links + j, ctx->links + i, sizeof(DMA_MEM_LINK));
                j++;
            }
        }
        ctx->n_links = j;
    } else {
        // If it is not known, update the links
        for (int i=0; i < ctx->n_links; i++) {
            if ((ctx->links[i].value_name) && (strcmp(ctx->links[i].value_name, "__end__") == 0) && (ctx->links[i].dest_ptr)) {
                ctx->links[i].value_name = target.name;
                ctx->links[i].value_offset = target.offset;
            }
        }
    }
}

void cc_ret(DMA_CTX *ctx) {
    cc_goto(ctx, CC_CREF("__ret__"));
}

DMA_CB *cc_clean(DMA_CTX *pctx, DMA_CTX *ctx) {
    //printf("Cleaning up ctx has %d cbs, %d labels, %d links\n", ctx->n_cbs, ctx->n_labels, ctx->n_links);
    int i, j;
    DMA_MEM_LINK *link;
    DMA_MEM_LABEL *label;
    for (i=0; i < ctx->n_links; i++) {
        link = ctx->links + i;
        if ((link->value_name) && ((strcmp(link->value_name, "__end__") == 0) || (strcmp(link->value_name, "__ret__") == 0))) {
            link->value_name = "__end__";
            if (pctx) {
                if (pctx->n_links >= pctx->s_links) {
                    pctx->s_links += 16;
                    pctx->links = realloc(pctx->links, pctx->s_links * sizeof(DMA_MEM_LINK));
                }
                memcpy(pctx->links + pctx->n_links, link, sizeof(DMA_MEM_LINK));
                pctx->n_links++;
            }
        } else {
            // Look for the destination first. If it is not found, throw out the link
            if (!(link->dest_ptr)) {
                for (j=0; j < ctx->n_labels; j++) {
                    label = ctx->labels + j;
                    if (strcmp(label->name, link->dest_name)) {
                        link->dest_ptr = label->ptr;
                        break;
                    }
                }
                if (j == ctx->n_labels) {
                    printf("Invalid link %s/%p + %d (%08x) to %s/%p + %d\n", link->value_name, link->value_ptr, link->value_offset,
                                                                           MEM_BUS_ADDR(ctx->mp, link->value_ptr + link->value_offset),
                                                                           link->dest_name, link->dest_ptr, link->dest_offset);
                }
            }
            if (!(link->value_ptr)) {
                for (j=0; j < ctx->n_labels; j++) {
                    label = ctx->labels + j;
                    if (strcmp(label->name, link->value_name) == 0) {
                        link->value_ptr = label->ptr;
                        break;
                    }
                }
                if (j == ctx->n_labels) {
                    if (!pctx) printf("Unresolved link %s\n", link->value_name);
                    else {
                        if (pctx->n_links >= pctx->s_links) {
                            pctx->s_links += 16;
                            pctx->links = realloc(pctx->links, pctx->s_links * sizeof(DMA_MEM_LINK));
                        }
                        memcpy(pctx->links + pctx->n_links, link, sizeof(DMA_MEM_LINK));
                        pctx->n_links++;
                    }
                }
            }
            //printf("Connecting %s/%p + %d (%08x) to %s/%p + %d\n", link->value_name, link->value_ptr, link->value_offset,
            //                                                       MEM_BUS_ADDR(ctx->mp, link->value_ptr + link->value_offset),
            //                                                       link->dest_name, link->dest_ptr, link->dest_offset);
            *((uint32_t*)(link->dest_ptr + link->dest_offset)) = MEM_BUS_ADDR(ctx->mp, link->value_ptr + link->value_offset);
        }
    }
    if (pctx) pctx->n_cbs += ctx->n_cbs;
    DMA_CB *r = ctx->start_cb;
    //printf("freeing %p, %p, and %p\n", ctx->labels, ctx->links, ctx);
    free(ctx->labels);
    free(ctx->links);
    free(ctx);
    return r;
}

DMA_MEM_REF cc_ofs(DMA_MEM_REF src, int32_t n) {
    src.offset += n;
    return src;
}

DMA_MEM_REF new_ref(char *name, void *ptr, int32_t offset) {
    DMA_MEM_REF r;
    r.name = name;
    r.ptr = ptr;
    r.offset = offset;
    return r;
}

