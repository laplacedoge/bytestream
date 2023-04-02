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

    /* byte stream buffer, which is used as a FIFO. */
    bstm_u8 *buff;

    /* head byte index. */
    bstm_u32 head_idx;

    /* tail byte index. */
    bstm_u32 tail_idx;

    /* configuration. */
    struct _bstm_ctx_conf {

        /* capacity of the byte stream. */
        bstm_u32 cap_size;
    } conf;

    /* cached data for fast access. */
    struct _bstm_ctx_cache {

        /* unused buffer size. */
        bstm_u32 free_size;

        /* used buffer size. */
        bstm_u32 used_size;
    } cache;
} bstm_ctx;

/* default capacity size. */
#define BSTM_DEF_CAP_SIZE   1024

/**
 * @brief create a new byte stream.
 * 
 * @param ctx context pointer.
 * @param conf configuration pointer.
*/
bstm_res bstm_new(bstm_ctx **ctx, bstm_conf *conf) {
    bstm_ctx *alloc_ctx;
    bstm_u8 *alloc_buff;
    bstm_u32 cap_size;
    bstm_u32 buff_size;

    BSTM_ASSERT(ctx != NULL);

    /* get capacity size. */
    if (conf != NULL) {
        cap_size = conf->cap_size;
    } else {
        cap_size = BSTM_DEF_CAP_SIZE;
    }

    /* make sure the buffer size is a multiple of 8. */
    buff_size = ((cap_size >> 3) + 1) << 3;

    /* allocate memory for the buffer. */
    alloc_buff = (bstm_u8 *)malloc(buff_size);
    if (alloc_buff == NULL) {
        return BSTM_ERR_NO_MEM;
    }

    /* allocate memory for the context. */
    alloc_ctx = (bstm_ctx *)malloc(sizeof(bstm_ctx));
    if (alloc_ctx == NULL) {
        free(alloc_buff);

        return BSTM_ERR_NO_MEM;
    }

    /* initialize the context. */
    memset(alloc_ctx, 0, sizeof(bstm_ctx));
    alloc_ctx->buff = alloc_buff;
    alloc_ctx->conf.cap_size = cap_size;
    alloc_ctx->cache.free_size = cap_size;

    /* return the context. */
    *ctx = alloc_ctx;

    return BSTM_OK;
}

/**
 * @brief delete the byte stream.
 * 
 * @param ctx context pointer.
*/
bstm_res bstm_del(bstm_ctx *ctx) {
    BSTM_ASSERT(ctx != NULL);

    /* free the buffer and the context. */
    free(ctx->buff);
    free(ctx);

    return BSTM_OK;
}

/**
 * @brief get the status of the byte stream.
 * 
 * @param ctx context pointer.
 * @param stat status pointer.
*/
bstm_res bstm_status(bstm_ctx *ctx, bstm_stat *stat) {
    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(stat != NULL);

    /* get the status. */
    stat->cap_size = ctx->conf.cap_size;
    stat->free_size = ctx->cache.free_size;
    stat->used_size = ctx->cache.used_size;

    return BSTM_OK;
}

/**
 * @brief write data to the byte stream.
 * 
 * @param ctx context pointer.
 * @param buff buffer pointer.
 * @param size data size.
*/
bstm_res bstm_write(bstm_ctx *ctx, const void *buff, bstm_u32 size) {
    bstm_u8 *first_copy_ptr;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(buff != NULL);

    /* if the size is 0, return immediately. */
    if (size == 0) {
        return BSTM_OK;
    }

    /* check if there is enough space. */
    if (ctx->cache.free_size < size) {
        return BSTM_ERR_NO_SPA;
    }

    /* copy the external buffer to the byte stream buffer and update the tail
       index. */
    first_copy_ptr = ctx->buff + ctx->tail_idx;
    if (ctx->conf.cap_size + 1 - ctx->tail_idx >= size) {
        memcpy(first_copy_ptr, buff, size);
        ctx->tail_idx = (ctx->tail_idx + size) % (ctx->conf.cap_size + 1);
    } else {
        bstm_u32 first_copy_size = ctx->conf.cap_size + 1 - ctx->tail_idx;
        bstm_u32 second_copy_size = size - first_copy_size;

        memcpy(first_copy_ptr, buff, first_copy_size);
        memcpy(ctx->buff, (bstm_u8 *)buff + first_copy_size, second_copy_size);
        ctx->tail_idx = second_copy_size;
    }

    /* update the cache. */
    ctx->cache.used_size += size;
    ctx->cache.free_size -= size;

    return BSTM_OK;
}

/**
 * @brief read data from the byte stream.
 * 
 * @note if the buffer pointer is NULL, the data will be discarded without
 *       buffer copying.
 * 
 * @param ctx context pointer.
 * @param buff buffer pointer.
 * @param size data size.
*/
bstm_res bstm_read(bstm_ctx *ctx, void *buff, bstm_u32 size) {
    bstm_u8 *first_copy_ptr;

    BSTM_ASSERT(ctx != NULL);

    /* if the size is 0, return immediately. */
    if (size == 0) {
        return BSTM_OK;
    }

    /* check if there is enough data. */
    if (ctx->cache.used_size < size) {
        return BSTM_ERR_NO_DAT;
    }

    /* copy the data from the byte stream buffer to the external buffer and
       update the head index. */
    first_copy_ptr = (bstm_u8 *)ctx->buff + ctx->head_idx;
    if (ctx->conf.cap_size + 1 - ctx->head_idx >= size) {
        if (buff != NULL) {
            memcpy(buff, first_copy_ptr, size);
        }
        ctx->head_idx = (ctx->head_idx + size) % (ctx->conf.cap_size + 1);
    } else {
        bstm_u32 first_copy_size = ctx->conf.cap_size + 1 - ctx->head_idx;
        bstm_u32 second_copy_size = size - first_copy_size;

        if (buff != NULL) {
            memcpy(buff, first_copy_ptr, first_copy_size);
            memcpy((bstm_u8 *)buff + first_copy_size, ctx->buff, second_copy_size);
        }

        ctx->head_idx = second_copy_size;
    }

    /* update the cache. */
    ctx->cache.used_size -= size;
    ctx->cache.free_size += size;

    return BSTM_OK;
}

/**
 * @brief peek data from the byte stream.
 * 
 * @note the data will not be removed from the byte stream.
 * 
 * @param ctx context pointer.
 * @param buff data buffer.
 * @param offs offset.
 * @param size data size.
*/
bstm_res bstm_peek(bstm_ctx *ctx, void *buff, bstm_u32 offs, bstm_u32 size) {
    bstm_u8 *first_copy_ptr;
    bstm_u32 temp_head_idx;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(buff != NULL);

    /* check if the offset is valid. */
    if (offs >= ctx->cache.used_size) {
        return BSTM_ERR_BAD_OFFS;
    }

    /* if the size is 0, return immediately. */
    if (size == 0) {
        return BSTM_OK;
    }

    /* check if there is enough data. */
    if (ctx->cache.used_size < offs + size) {
        return BSTM_ERR_NO_DAT;
    }

    /* copy the data from the byte stream buffer to the external buffer. */
    temp_head_idx = (ctx->head_idx + offs) % (ctx->conf.cap_size + 1);
    first_copy_ptr = (bstm_u8 *)ctx->buff + temp_head_idx;
    if (ctx->conf.cap_size + 1 - temp_head_idx >= size) {
        memcpy(buff, first_copy_ptr, size);
    } else {
        bstm_u32 first_copy_size = ctx->conf.cap_size + 1 - temp_head_idx;
        bstm_u32 second_copy_size = size - first_copy_size;

        memcpy(buff, first_copy_ptr, first_copy_size);
        memcpy((bstm_u8 *)buff + first_copy_size, ctx->buff, second_copy_size);
    }

    return BSTM_OK;
}
