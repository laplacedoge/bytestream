/**
 * MIT License
 * 
 * Copyright (c) 2023 Alex Chen
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "bytestream.h"

/* context of the byte stream. */
typedef struct _bstm_ctx {
    bstm_u32 cap;
    bstm_u32 size;
    bstm_u8 *buff;
    bstm_u32 head;
    bstm_u32 tail;
    struct _bstm_ctx_cache
    {
        bstm_u32 free;
        bstm_u32 used;
    } cache;
} bstm_ctx;

/* the maximum capacity of the byte stream. */
#define BSTM_CAP_MAX    (0xFFFFFFFF - 8)

bstm_res bstm_new(bstm_ctx **ctx, bstm_u32 cap) {
    bstm_ctx *alloc_ctx;
    bstm_u8 *alloc_buff;
    bstm_u32 buff_size;
    bstm_res res;
    int ret;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(cap != 0);
    BSTM_ASSERT(cap <= BSTM_CAP_MAX);

    /* allocate context. */
    alloc_ctx = (bstm_ctx *)malloc(sizeof(bstm_ctx));
    if (alloc_ctx == NULL)
    {
        return BSTM_ERR_NO_MEM;
    }

    /* allocate buffer. */
    buff_size = ((cap >> 3) + 1) << 3;
    alloc_buff = (bstm_u8 *)malloc(alloc_ctx->size);
    if (alloc_buff == NULL)
    {
        free(alloc_ctx);
        return BSTM_ERR_NO_MEM;
    }

    /* initialize context. */
    alloc_ctx->cap = cap;
    alloc_ctx->size = buff_size;
    alloc_ctx->buff = alloc_buff;
    alloc_ctx->head = 0;
    alloc_ctx->tail = 0;
    alloc_ctx->cache.free = cap;
    alloc_ctx->cache.used = 0;
    *ctx = alloc_ctx;

    return BSTM_OK;
}

bstm_res bstm_del(bstm_ctx *ctx) {
    BSTM_ASSERT(ctx != NULL);

    free(ctx->buff);
    free(ctx);

    return BSTM_OK;
}

bstm_res bstm_status(bstm_ctx *ctx, bstm_stat *stat) {
    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(stat != NULL);

    stat->cap = ctx->cap;
    stat->free = ctx->cache.free;
    stat->used = ctx->cache.used;

    return BSTM_OK;
}

bstm_res bstm_write(bstm_ctx *ctx, const void *buff, bstm_u32 size) {
    bstm_u8 *first_copy_ptr;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(buff != NULL);
    BSTM_ASSERT(size <= BSTM_CAP_MAX);

    if (size == 0)
    {
        return BSTM_OK;
    }

    if (ctx->cache.free < size)
    {
        return BSTM_ERR_NO_SPA;
    }

    first_copy_ptr = ctx->buff + ctx->tail;

    /* check if we can copy it all at once. */
    if (ctx->cap + 1 - ctx->tail >= size)
    {
        memcpy(first_copy_ptr, buff, size);
        ctx->tail = (ctx->tail + size) % (ctx->cap + 1);
    }
    else
    {
        bstm_u32 first_copy_size = ctx->cap + 1 - ctx->tail;
        bstm_u32 second_copy_size = size - first_copy_size;

        /* first copy. */
        memcpy(first_copy_ptr, buff, first_copy_size);

        /* second copy. */
        memcpy(ctx->buff, (bstm_u8 *)buff + first_copy_size, second_copy_size);

        ctx->tail = second_copy_size;
    }

    ctx->cache.used += size;
    ctx->cache.free = ctx->cap - ctx->cache.used;

    return BSTM_OK;
}

bstm_res bstm_read(bstm_ctx *ctx, void *buff, bstm_u32 size) {
    bstm_u8 *first_copy_ptr;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(size <= BSTM_CAP_MAX);

    if (size == 0)
    {
        return BSTM_OK;
    }

    if (ctx->cache.used < size)
    {
        return BSTM_ERR_NO_DAT;
    }

    first_copy_ptr = (bstm_u8 *)ctx->buff + ctx->head;

    /* check if we can copy it all at once. */
    if (ctx->cap + 1 - ctx->head >= size)
    {
        if (buff != NULL)
        {
            memcpy(buff, first_copy_ptr, size);
        }
        ctx->head = (ctx->head + size) % (ctx->cap + 1);
    }
    else
    {
        bstm_u32 first_copy_size = ctx->cap + 1 - ctx->head;
        bstm_u32 second_copy_size = size - first_copy_size;

        if (buff != NULL)
        {
            /* first copy. */
            memcpy(buff, first_copy_ptr, first_copy_size);

            /* second copy. */
            memcpy((bstm_u8 *)buff + first_copy_size, ctx->buff, second_copy_size);
        }

        ctx->head = second_copy_size;
    }

    ctx->cache.used -= size;
    ctx->cache.free = ctx->cap - ctx->cache.used;

    return BSTM_OK;
}

bstm_peek(bstm_ctx *ctx, void *buff, bstm_u32 offs, bstm_u32 size) {
    bstm_u8 *first_copy_ptr;
    bstm_u32 temp_head;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(buff != NULL);
    BSTM_ASSERT(size <= BSTM_CAP_MAX);

    if (size == 0)
    {
        return BSTM_OK;
    }

    if (ctx->cache.used < offs + size)
    {
        return BSTM_ERR_NO_DAT;
    }

    temp_head = (ctx->head + offs) % (ctx->cap + 1);
    first_copy_ptr = (bstm_u8 *)ctx->buff + temp_head;

    /* check if we can copy it all at once. */
    if (ctx->cap + 1 - temp_head >= size)
    {
        memcpy(buff, first_copy_ptr, size);
    }
    else
    {
        bstm_u32 first_copy_size = ctx->cap + 1 - temp_head;
        bstm_u32 second_copy_size = size - first_copy_size;

        /* first copy. */
        memcpy(buff, first_copy_ptr, first_copy_size);

        /* second copy. */
        memcpy((bstm_u8 *)buff + first_copy_size, ctx->buff, second_copy_size);
    }

    return BSTM_OK;
}
