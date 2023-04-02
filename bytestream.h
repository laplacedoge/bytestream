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

#ifndef __BSTM_H__
#define __BSTM_H__

#ifdef BSTM_DEBUG

#include <stdarg.h>
#include <stdio.h>

#endif

/* basic data types. */
typedef signed char     bstm_s8;
typedef unsigned char   bstm_u8;

typedef signed short    bstm_s16;
typedef unsigned short  bstm_u16;

typedef signed int      bstm_s32;
typedef unsigned int    bstm_u32;

enum _bstm_res {

    /* all is well, so far :) */
    BSTM_OK             = 0,

    /* generic error occurred. */
    BSTM_ERR            = -1,

    /* failed to allocate memory. */
    BSTM_ERR_NO_MEM     = -2,

    /* insufficient space to write data. */
    BSTM_ERR_NO_SPA     = -3,

    /* insufficient data to read or peek. */
    BSTM_ERR_NO_DAT     = -4,

    /* invalid offset. */
    BSTM_ERR_BAD_OFFS   = -5,
};

#ifdef BSTM_DEBUG

/* logging function for debug. */
#define BSTM_LOG(fmt, ...)  printf(fmt, ##__VA_ARGS__)

/* assertion macro used in the APIs. */
#define BSTM_ASSERT(expr)   \
    if (!(expr)) { BSTM_LOG("[BSTM] %s:%d: assertion failed: \"%s\"\n", \
        __FILE__, __LINE__, #expr); while (1);};

/* returned result used in the APIs. */
typedef enum _bstm_res      bstm_res;

#else

/* returned result used in the APIs. */
typedef bstm_s32            bstm_res;

/* assertion macro used in the APIs. */
#define BSTM_ASSERT(expr)

#endif

/* context of the byte stream. */
typedef struct _bstm_ctx    bstm_ctx;

/* configuration of the byte stream. */
typedef struct _bstm_conf {

    /* capacity of the byte stream. */
    bstm_u32 cap_size;
} bstm_conf;

/* status of the byte stream. */
typedef struct _bstm_stat {

    /* capacity of the byte stream. */
    bstm_u32 cap_size;

    /* free space size. */
    bstm_u32 free_size;

    /* used space size. */
    bstm_u32 used_size;
} bstm_stat;

bstm_res bstm_new(bstm_ctx **ctx, bstm_conf *conf);

bstm_res bstm_del(bstm_ctx *ctx);

bstm_res bstm_status(bstm_ctx *ctx, bstm_stat *stat);

bstm_res bstm_write(bstm_ctx *ctx, const void *buff, bstm_u32 size);

bstm_res bstm_read(bstm_ctx *ctx, void *buff, bstm_u32 size);

bstm_res bstm_peek(bstm_ctx *ctx, void *buff, bstm_u32 offs, bstm_u32 size);

#endif
