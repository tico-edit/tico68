/*
 * pool.c - the allocator behind lines and their text
 *
 * Retro68's malloc is NewPtr, which costs an 8-byte header per block and
 * walks the heap to place each one.  A file is two blocks per line, so
 * small blocks are carved from 16K chunks instead, with a free list for
 * each 8-byte size class.  Callers say how big a block is when they free
 * it, so blocks have no headers at all.  Chunks are never given back,
 * but what is freed in them is reused by the next document.
 *
 * Portable C; plat_alloc and plat_free are the host's.
 */

#include <string.h>
#include "tico.h"

#define POOL_CHUNK 16384u
#define POOL_BIG   512u         /* larger blocks go straight to the host */
#define CLASS(n)   (((n) + 7u) >> 3)

typedef struct freeblk {
    struct freeblk *next;
} freeblk;

static freeblk *freelist[CLASS(POOL_BIG) + 1];
static char *chunk;
static unsigned chunk_left;

unsigned pool_round(unsigned size)
{
    if (size > POOL_BIG)
        return size;
    return CLASS(size) << 3;
}

void pool_free(void *p, unsigned size)
{
    freeblk *b = p;

    if (p == NULL)
        return;
    if (size > POOL_BIG) {
        plat_free(p);
        return;
    }
    b->next = freelist[CLASS(size)];
    freelist[CLASS(size)] = b;
}

void *pool_alloc(unsigned size)
{
    unsigned c, need;
    void *p;

    if (size > POOL_BIG)
        return plat_alloc(size);
    c = CLASS(size);
    if (c == 0)
        c = 1;
    if (freelist[c] != NULL) {
        p = freelist[c];
        freelist[c] = freelist[c]->next;
        return p;
    }
    need = c << 3;
    if (chunk_left < need) {
        p = plat_alloc(POOL_CHUNK);
        if (p == NULL)
            return NULL;
        /* what's left of the old chunk is a free block of some class */
        if (chunk_left > 0)
            pool_free(chunk, chunk_left);
        chunk = p;
        chunk_left = POOL_CHUNK;
    }
    p = chunk;
    chunk += need;
    chunk_left -= need;
    return p;
}
