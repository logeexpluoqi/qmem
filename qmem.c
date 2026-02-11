/**
 * Author: luoqi
 * Created Date: 2026-01-12 00:12:3
 * Last Modified: 2026-02-12 00:25:18
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2026 <*****>
 * Description:
 */
 
#include "qmem.h"

static inline size_t align_up(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

static inline int block_valid(QMem *mem, QMemBlock *block)
{
    if(!mem || !block) {
        return 0;
    }
    if(block->magic != mem->magic) {
        return 0;
    }
    if(block->size < sizeof(QMemBlock)) {
        return 0;
    }
    // Additional check: ensure 'prev' and 'next' (if exist) also have valid magic?
    // Not done here for performance, but current checks are minimal.
    return 1;
}

#if QMEM_USING_STDLIBC_

#include <string.h>
#define _memcpy memcpy

#else

static void *_memcpy(void *dest, const void *src, size_t size)
{
    if(!dest || !src || size == 0) {
        return dest;
    }

    uint8_t *d = dest;
    const uint8_t *s = src;
    for(size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

#endif

static void *_qmem_alloc(QMem *mem, size_t size)
{
    if(!mem || (size == 0) || !mem->blocks) {
        return NULL;
    }

    size_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    if(total_size < size || total_size < sizeof(QMemBlock)) {
        return NULL;
    }
    QMemBlock *current = mem->blocks;

    // first fit
    while(current) {
        if(!current->used && current->size >= total_size) {
            if(current->size >= total_size + mem->min_split) {
                QMemBlock *new_block = (QMemBlock *)((uint8_t *)current + total_size);
                new_block->size = current->size - total_size;
                new_block->used = 0;
                new_block->magic = mem->magic;
                new_block->next = current->next;
                new_block->prev = current;
                if(current->next) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = total_size;
            }

            current->used = 1;
            mem->total_free -= total_size;
            if(total_size > mem->max_block) {
                mem->max_block = total_size;
            }
            return (void *)((uint8_t *)current + sizeof(QMemBlock));
        }
        current = current->next;
    }
    return NULL;
}

static int _qmem_free(QMem *mem, void *ptr)
{
    if(!mem || !ptr) {
        return -1;
    }

    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    if(!block_valid(mem, header)) {
        return -1;
    }

    header->used = 0;

    // merge with prev
    if(header->prev && !header->prev->used &&
        ((uint8_t *)header->prev + header->prev->size == (uint8_t *)header)) {
        header->prev->size += header->size;
        header->prev->next = header->next;
        if(header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    }

    if(header->next && !header->next->used &&
        ((uint8_t *)header + header->size == (uint8_t *)header->next)) {
        header->size += header->next->size;
        header->next = header->next->next;
        if(header->next) {
            header->next->prev = header;
        }
    }
    mem->total_free += header->size;
    return 0;
}

int qmem_init(QMem *mem, void *mempool, size_t size, size_t align, size_t min_split, uint8_t magic, void (*lock)(bool))
{
    if(!mem || !mempool) {
        return -1;
    }
    mem->align = align;
    mem->min_split = min_split;
    mem->magic = magic;
    mem->max_block = 0;
    mem->total_free = size;
    mem->blocks = (QMemBlock *)mempool;
    mem->blocks->size = size;
    mem->blocks->used = 0;
    mem->blocks->next = NULL;
    mem->blocks->prev = NULL;
    mem->blocks->magic = magic;
    mem->lock = lock;
    return 0;
}

void *qmem_alloc(QMem *mem, size_t size)
{
    if(!mem || (size == 0) || !mem->blocks) {
        return NULL;
    }
    if(mem->lock) {
        mem->lock(true);
    }
    void *ret = _qmem_alloc(mem, size);
    if(mem->lock) {
        mem->lock(false);
    }
    return ret;
}

void *qmem_realloc(QMem *mem, void *ptr, size_t size)
{
    if(!mem) {
        return NULL;
    }

    if(mem->lock) {
        mem->lock(true);
    }

    if(ptr == NULL) {
        void *ret = _qmem_alloc(mem, size);
        if(mem->lock) {
            mem->lock(false);
        }
        return ret;
    }

    if(size == 0) {
        _qmem_free(mem, ptr);
        if(mem->lock) {
            mem->lock(false);
        }
        return NULL;
    }

    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    if(!block_valid(mem, header)) {
        if(mem->lock) {
            mem->lock(false);
        }
        return NULL;
    }

    size_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    if(total_size < size || total_size < sizeof(QMemBlock)) {
        if(mem->lock) {
            mem->lock(false);
        }
        return NULL;
    }

    if(header->size >= total_size) {
        if(mem->lock) {
            mem->lock(false);
        }
        return ptr;
    }

    // Allocate new block without re-acquiring lock
    void *new_ptr = _qmem_alloc(mem, size);
    if(new_ptr == NULL) {
        if(mem->lock) {
            mem->lock(false);
        }
        return NULL;
    }

    // Copy only the user-requested original data size (not full block payload)
    // Note: original user size is unknown, so we use min(old_payload, new_size)
    size_t old_payload_size = header->size - sizeof(QMemBlock);
    size_t copy_size = (old_payload_size < size) ? old_payload_size : size;
    _memcpy(new_ptr, ptr, copy_size);

    // Free old block without re-acquiring lock
    _qmem_free(mem, ptr);

    if(mem->lock) {
        mem->lock(false);
    }
    return new_ptr;
}

int qmem_free(QMem *mem, void *ptr)
{
    if(!mem || !ptr) {
        return -1;
    }
    if(mem->lock) {
        mem->lock(true);
    }
    int ret = _qmem_free(mem, ptr);
    if(mem->lock) {
        mem->lock(false);
    }
    return ret;
}

int qmem_defrag(QMem *mem)
{
    if(!mem) {
        return -1;
    }
    if(mem->lock) {
        mem->lock(true);
    }
    QMemBlock *current = mem->blocks;
    while(current && current->next) {
        if(!current->used && !current->next->used &&
           ((uint8_t *)current + current->size == (uint8_t *)current->next)) {
            current->size += current->next->size;
            current->next = current->next->next;
            if(current->next) {
                current->next->prev = current;
            }
        } else {
            current = current->next;
        }
    }
    if(mem->lock) {
        mem->lock(false);
    }
    return 0;
}

int qmem_status(QMem *mem)
{
    if(!mem) {
        return -1;
    }
    mem->max_block = 0;
    mem->total_free = 0;

    QMemBlock *current = mem->blocks;
    while(current) {
        if(!current->used) {
            mem->total_free += current->size;
        }
        if(current->size > mem->max_block) {
            mem->max_block = current->size;
        }
        current = current->next;
    }
    return 0;
}
