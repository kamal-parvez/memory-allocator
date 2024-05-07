// block_meta.h
#ifndef BLOCK_META_H
#define BLOCK_META_H

#include <stddef.h>

typedef struct block_meta {
    size_t size;
    int free;
    struct block_meta* next;
} block_meta;

#endif // BLOCK_META_H
