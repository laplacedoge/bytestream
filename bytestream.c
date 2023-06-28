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

    /* ring buffer. */
    bstm_u8_t *ring_buff;

    /* head byte index. */
    bstm_u32_t head_idx;

    /* tail byte index. */
    bstm_u32_t tail_idx;

    /* configuration. */
    struct _bstm_ctx_conf {

        /* capacity of the byte stream. */
        bstm_u32_t cap_size;
    } conf;

    /* cached data for fast access. */
    struct _bstm_ctx_cache {

        /* unused buffer size. */
        bstm_u32_t free_size;

        /* used buffer size. */
        bstm_u32_t used_size;
    } cache;
} bstm_ctx_t;

/* default capacity size. */
#define BSTM_DEF_CAP_SIZE   1024

/**
 * @brief create a new byte stream.
 * 
 * @param ctx context pointer.
 * @param conf configuration pointer.
*/
bstm_res_t bstm_new(bstm_ctx_t **ctx, bstm_conf_t *conf) {
    bstm_ctx_t *alloc_ctx;
    bstm_u8_t *alloc_buff;
    bstm_u32_t cap_size;
    bstm_u32_t buff_size;

    BSTM_ASSERT(ctx != NULL);

    /* get capacity size. */
    if (conf != NULL) {
        cap_size = conf->cap_size;
    } else {
        cap_size = BSTM_DEF_CAP_SIZE;
    }

    /* make sure the buffer size is a multiple of 8. */
    buff_size = ((cap_size >> 3) + 1) << 3;

    /* allocate memory for the ring buffer. */
    alloc_buff = (bstm_u8_t *)malloc(buff_size);
    if (alloc_buff == NULL) {
        return BSTM_ERR_NO_MEM;
    }

    /* allocate memory for the context. */
    alloc_ctx = (bstm_ctx_t *)malloc(sizeof(bstm_ctx_t));
    if (alloc_ctx == NULL) {
        free(alloc_buff);

        return BSTM_ERR_NO_MEM;
    }

    /* initialize the context. */
    memset(alloc_ctx, 0, sizeof(bstm_ctx_t));
    alloc_ctx->ring_buff = alloc_buff;
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
bstm_res_t bstm_del(bstm_ctx_t *ctx) {
    BSTM_ASSERT(ctx != NULL);

    /* free the buffer and the context. */
    free(ctx->ring_buff);
    free(ctx);

    return BSTM_OK;
}

/**
 * @brief get the status of the byte stream.
 * 
 * @param ctx context pointer.
 * @param stat status pointer.
*/
bstm_res_t bstm_stat(bstm_ctx_t *ctx, bstm_stat_t *stat) {
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
 * @param data data pointer.
 * @param size data size.
*/
bstm_res_t bstm_write(bstm_ctx_t *ctx, const void *data, bstm_size_t size) {
    bstm_u8_t *first_copy_ptr;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(data != NULL);

    /* if the size is 0, return immediately. */
    if (size == 0) {
        return BSTM_OK;
    }

    /* check if there is enough space. */
    if (ctx->cache.free_size < size) {
        return BSTM_ERR_NO_SPA;
    }

    /* copy data to the ring buffer. */
    first_copy_ptr = ctx->ring_buff + ctx->tail_idx;
    if (ctx->conf.cap_size + 1 - ctx->tail_idx >= size) {
        memcpy(first_copy_ptr, data, size);
        ctx->tail_idx = (ctx->tail_idx + size) % (ctx->conf.cap_size + 1);
    } else {
        bstm_size_t first_copy_size = ctx->conf.cap_size + 1 - ctx->tail_idx;
        bstm_size_t second_copy_size = size - first_copy_size;

        memcpy(first_copy_ptr, data, first_copy_size);
        memcpy(ctx->ring_buff, (bstm_u8_t *)data + first_copy_size, second_copy_size);
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
 * @note if the data pointer is NULL, the data will be discarded without any
 *       copying.
 * 
 * @param ctx context pointer.
 * @param data data pointer.
 * @param size data size.
*/
bstm_res_t bstm_read(bstm_ctx_t *ctx, void *data, bstm_size_t size) {
    bstm_u8_t *first_copy_ptr;

    BSTM_ASSERT(ctx != NULL);

    /* if the size is 0, return immediately. */
    if (size == 0) {
        return BSTM_OK;
    }

    /* check if there is enough data. */
    if (ctx->cache.used_size < size) {
        return BSTM_ERR_NO_DAT;
    }

    /* copy data from the ring buffer. */
    first_copy_ptr = (bstm_u8_t *)ctx->ring_buff + ctx->head_idx;
    if (ctx->conf.cap_size + 1 - ctx->head_idx >= size) {
        if (data != NULL) {
            memcpy(data, first_copy_ptr, size);
        }
        ctx->head_idx = (ctx->head_idx + size) % (ctx->conf.cap_size + 1);
    } else {
        bstm_size_t first_copy_size = ctx->conf.cap_size + 1 - ctx->head_idx;
        bstm_size_t second_copy_size = size - first_copy_size;

        if (data != NULL) {
            memcpy(data, first_copy_ptr, first_copy_size);
            memcpy((bstm_u8_t *)data + first_copy_size, ctx->ring_buff, second_copy_size);
        }

        ctx->head_idx = second_copy_size;
    }

    /* update the cache. */
    ctx->cache.used_size -= size;
    ctx->cache.free_size += size;

    return BSTM_OK;
}

typedef enum _eol {
    EOL_NONE    = 0,
    EOL_CR      = 1,
    EOL_LF      = 2,
    EOL_CRLF    = 3,
} eol_t;

/**
 * @brief find EOL in data buffer.
 * 
 * @note CR, LF, and CRLF are both suppored.
 * 
 * @param data data pointer.
 * @param size data size.
 * @param len line length pointer.
*/
static eol_t find_eol(const void *data, bstm_size_t size, bstm_size_t *len) {
    bstm_size_t rest_size;
    bstm_u8_t byte;
    bstm_u8_t *data_p;

    BSTM_ASSERT(data != NULL);
    BSTM_ASSERT(size != 0);
    BSTM_ASSERT(len != NULL);

    rest_size = size;
    data_p = (bstm_u8_t *)data;
    while (rest_size > 0) {
        byte = *data_p;
        if (byte == '\r') {
            if (rest_size - 1 >= 1) {
                byte = *(data_p + 1);
                if (byte == '\n') {
                    *len = size - rest_size + 2;

                    return EOL_CRLF;
                }
            }
            *len = size - rest_size + 1;

            return EOL_CR;
        } else if (byte == '\n') {
            *len = size - rest_size + 1;

            return EOL_LF;
        }

        rest_size--;
        data_p++;
    }
    *len = 0;

    return EOL_NONE;
}

/**
 * @brief reads a line of data from the byte stream.
 * 
 * @note if data is NULL, then the line data won't be removed from byte stream.
 *       if len is NULL, then line length won't be sent back to the caller.
 *       however, data and len must not be both NULL at the same time.
 * 
 * @param ctx context pointer.
 * @param data data pointer.
 * @param size data buffer size.
 * @param len line length pointer.
 * 
 * @return BSTM_OK              read line data successfully.
 *         BSTM_ERR             generic error.
 *         BSTM_ERR_BAD_SIZE    the data buffer size is insufficient for the incoming line data.
 *         BSTM_ERR_NO_EOL      can't find any kind of EOL character in current byte stream.
 * 
*/
bstm_res_t bstm_readline(bstm_ctx_t *ctx, void *data, bstm_size_t size, bstm_size_t *len) {
    bstm_u8_t *buff_1st_part_ptr;
    bstm_size_t head_to_buff_end_size;
    bstm_size_t line_size;

    BSTM_ASSERT(ctx != NULL);

    /* data and len must not be both NULL at the same time. */
    if (data == NULL &&
        len == NULL) {
        return BSTM_ERR;
    }

    /* a buffer of at least 1 byte length is required. */
    if (size == 0) {
        return BSTM_ERR_BAD_SIZE;
    }

    if (ctx->cache.used_size == 0) {
        return BSTM_ERR_NO_EOL;
    }

    buff_1st_part_ptr = ctx->ring_buff + ctx->head_idx;
    head_to_buff_end_size = ctx->conf.cap_size + 1 - ctx->head_idx;
    if (ctx->cache.used_size <= head_to_buff_end_size) {
        eol_t eol;

        eol = find_eol(buff_1st_part_ptr, ctx->cache.used_size, &line_size);
        if (eol == EOL_NONE) {
            return BSTM_ERR_NO_EOL;
        }

        if (line_size > size) {
            return BSTM_ERR_BAD_SIZE;
        }

        if (data != NULL) {
            memcpy(data, buff_1st_part_ptr, line_size);

            ctx->head_idx = (ctx->head_idx + line_size) % (ctx->conf.cap_size + 1);
        }
    } else {
        bstm_size_t buff_1st_part_size;
        bstm_size_t buff_2nd_part_size;
        eol_t eol;

        buff_1st_part_size = head_to_buff_end_size;
        buff_2nd_part_size = ctx->tail_idx;
        eol = find_eol(ctx->ring_buff + ctx->head_idx, buff_1st_part_size, &line_size);
        if (eol == EOL_NONE) {
            bstm_size_t line_2nd_part_size;

            eol = find_eol(ctx->ring_buff, buff_2nd_part_size, &line_2nd_part_size);
            if (eol == EOL_NONE) {
                return BSTM_ERR_NO_EOL;
            }

            if (data != NULL) {
                memcpy(data, buff_1st_part_ptr, buff_1st_part_size);
                memcpy(data + buff_1st_part_size, ctx->ring_buff, line_2nd_part_size);

                ctx->head_idx = line_2nd_part_size;
            }

            line_size = buff_1st_part_size + line_2nd_part_size;
        } else if (eol == EOL_CR &&
                   line_size == buff_1st_part_size) {
            if (ctx->ring_buff[0] == '\n') {
                line_size++;

                if (line_size > size) {
                    return BSTM_ERR_BAD_SIZE;
                }

                if (data != NULL) {
                    memcpy(data, buff_1st_part_ptr, buff_1st_part_size);
                    *((bstm_u8_t *)data + buff_1st_part_size) = '\n';

                    ctx->head_idx = 1;
                }
            } else {
                if (line_size > size) {
                    return BSTM_ERR_BAD_SIZE;
                }

                if (data != NULL) {
                    memcpy(data, buff_1st_part_ptr, line_size);

                    ctx->head_idx = (ctx->head_idx + line_size) % (ctx->conf.cap_size + 1);
                }
            }
        } else {
            if (line_size > size) {
                return BSTM_ERR_BAD_SIZE;
            }

            if (data != NULL) {
                memcpy(data, buff_1st_part_ptr, line_size);

                ctx->head_idx = (ctx->head_idx + line_size) % (ctx->conf.cap_size + 1);
            }
        }
    }
    if (len != NULL) {
        *len = line_size;
    }

    /* update the cache if needed. */
    if (data != NULL) {
        ctx->cache.used_size -= line_size;
        ctx->cache.free_size += line_size;
    }

    return BSTM_OK;
}

/**
 * @brief peek data from the byte stream.
 * 
 * @note the data will not be removed from the byte stream.
 * 
 * @param ctx context pointer.
 * @param data data buffer.
 * @param offs offset.
 * @param size data size.
*/
bstm_res_t bstm_peek(bstm_ctx_t *ctx, void *data, bstm_size_t offs, bstm_size_t size) {
    bstm_u8_t *first_copy_ptr;
    bstm_size_t temp_head_idx;

    BSTM_ASSERT(ctx != NULL);
    BSTM_ASSERT(data != NULL);

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

    /* copy data from the ring buffer. */
    temp_head_idx = (ctx->head_idx + offs) % (ctx->conf.cap_size + 1);
    first_copy_ptr = (bstm_u8_t *)ctx->ring_buff + temp_head_idx;
    if (ctx->conf.cap_size + 1 - temp_head_idx >= size) {
        memcpy(data, first_copy_ptr, size);
    } else {
        bstm_size_t first_copy_size = ctx->conf.cap_size + 1 - temp_head_idx;
        bstm_size_t second_copy_size = size - first_copy_size;

        memcpy(data, first_copy_ptr, first_copy_size);
        memcpy((bstm_u8_t *)data + first_copy_size, ctx->ring_buff, second_copy_size);
    }

    return BSTM_OK;
}

/**
 * @brief clear all the data in the byte stream.
 * 
 * @param ctx context pointer.
 * 
 * @return BSTM_OK clear byte stream successfully.
*/
bstm_res_t bstm_clear(bstm_ctx_t *ctx) {
    BSTM_ASSERT(ctx != NULL);

    /* update indexes. */
    ctx->head_idx = 0;
    ctx->tail_idx = 0;

    /* update cache. */
    ctx->cache.free_size = ctx->conf.cap_size;
    ctx->cache.used_size = 0;

    return BSTM_OK;
}
