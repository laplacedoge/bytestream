# bytestream

A tiny byte stream library implemented in C.

## Usage

bytestream is easy to use, look:

```c
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "bytestream.h"

void show_bstm_status(bstm_ctx_t *stm) {
    bstm_stat_t stat;

    bstm_stat(stm, &stat);
    printf("capacity: %u, free: %u, used: %u\n",
        stat.cap, stat.free, stat.used);
}

int main(void) {
    bstm_ctx_t *stm;
    bstm_res_t res;
    char buff[128];

    /* create new bytestream. */
    res = bstm_new(&stm, 64);
    assert(res == BSTM_OK);
    show_bstm_status(stm);
    // capacity: 64, free: 64, used: 0

    /* write some data. */
    res = bstm_write(stm, "hello, world!", 13);
    assert(res == BSTM_OK);
    show_bstm_status(stm);
    // capacity: 64, free: 51, used: 13

    /* write more data. */
    res = bstm_write(stm, "this is bytestream library.", 27);
    assert(res == BSTM_OK);
    show_bstm_status(stm);
    // capacity: 64, free: 24, used: 40

    /* read some data and verify it. */
    res = bstm_read(stm, buff, 5);
    assert(res == BSTM_OK);
    assert(memcmp(buff, "hello", 5) == 0);
    show_bstm_status(stm);
    // capacity: 64, free: 29, used: 35

    /* try to read too much data. */
    res = bstm_read(stm, buff, 64);
    assert(res == BSTM_ERR_NO_DAT);
    show_bstm_status(stm);
    // capacity: 64, free: 29, used: 35

    /* try to read too much data. */
    res = bstm_read(stm, buff, 64);
    assert(res == BSTM_ERR_NO_DAT);
    show_bstm_status(stm);
    // capacity: 64, free: 29, used: 35

    /* try to write too much data. */
    res = bstm_write(stm, "let's try to crash this library!", 32);
    assert(res == BSTM_ERR_NO_SPA);
    show_bstm_status(stm);
    // capacity: 64, free: 29, used: 35

    /* try to peek some data, this operation
       won't affect the bytestream. */
    res = bstm_peek(stm, buff, 2, 5);
    assert(res == BSTM_OK);
    assert(memcmp(buff, "world", 5) == 0);
    show_bstm_status(stm);
    // capacity: 64, free: 29, used: 35

    /* delete the bytestream. */
    bstm_del(stm);

    return 0;
}
```
