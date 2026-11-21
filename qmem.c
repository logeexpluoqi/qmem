/**
 * Author: luoqi
 * Created Date: 2026-01-12 00:12:3
 * Last Modified: 2026-11-21 23:55:47
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
    uintptr_t pool_start = (uintptr_t)mem->pool_start;
    uintptr_t pool_end = (uintptr_t)mem->pool_end;
    uintptr_t block_addr = (uintptr_t)block;
    if(pool_end < pool_start || block_addr < pool_start ||
       block_addr > pool_end || pool_end - block_addr < sizeof(QMemBlock)) {
        return 0;
    }
    if(block->magic != mem->magic) {
        return 0;
    }
    if(block->size < sizeof(QMemBlock) ||
       block->size > pool_end - block_addr) {
        return 0;
    }
    if(!block->used) {
        return 0;
    }
    return 1;
}

static inline QMemBlock *block_from_ptr(QMem *mem, void *ptr)
{
    if(!mem || !ptr) {
        return NULL;
    }
    uintptr_t pool_start = (uintptr_t)mem->pool_start;
    uintptr_t pool_end = (uintptr_t)mem->pool_end;
    uintptr_t ptr_addr = (uintptr_t)ptr;
    if(pool_end < pool_start || ptr_addr < pool_start ||
       ptr_addr - pool_start < sizeof(QMemBlock) || ptr_addr >= pool_end) {
        return NULL;
    }
    QMemBlock *block = (QMemBlock *)(ptr_addr - sizeof(QMemBlock));
    return block_valid(mem, block) ? block : NULL;
}

#ifndef QMEM_LIBC_UNDEPENDENT

#include <string.h>
#define memcpy_ memcpy

#else

static void *memcpy_(void *dest, const void *src, size_t size)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if((((uintptr_t)d | (uintptr_t)s) & (sizeof(uint32_t) - 1)) == 0) {
        while(size >= sizeof(uint32_t)) {
            *(uint32_t *)d = *(const uint32_t *)s;
            d += sizeof(uint32_t);
            s += sizeof(uint32_t);
            size -= sizeof(uint32_t);
        }
    }
    while(size--) {
        *d++ = *s++;
    }
    return dest;
}

#endif

/* Split block at new_size, remainder becomes a free block.
   Caller must ensure: block->size > new_size && (block->size - new_size) >= sizeof(QMemBlock) */
static inline void block_split(QMemBlock *block, size_t new_size, uint8_t magic)
{
    QMemBlock *new_block = (QMemBlock *)((uint8_t *)block + new_size);
    new_block->size = block->size - new_size;
    new_block->used = 0;
    new_block->magic = magic;
    new_block->next = block->next;
    new_block->prev = block;
    if(block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = new_size;
}

static void *qmem_alloc_(QMem *mem, size_t size)
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
            if((current->size >= total_size + mem->min_split) &&
               ((current->size - total_size) >= sizeof(QMemBlock))) {
                block_split(current, total_size, mem->magic);
            }

            current->used = 1;
            mem->total_free -= current->size;
            return (void *)((uint8_t *)current + sizeof(QMemBlock));
        }
        current = current->next;
    }
    return NULL;
}

static int qmem_free_(QMem *mem, void *ptr)
{
    if(!mem || !ptr) {
        return -1;
    }

    QMemBlock *header = block_from_ptr(mem, ptr);
    if(!header) {
        return -1;
    }

    size_t freed_size = header->size;
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

    // merge with next
    if(header->next && !header->next->used &&
        ((uint8_t *)header + header->size == (uint8_t *)header->next)) {
        header->size += header->next->size;
        header->next = header->next->next;
        if(header->next) {
            header->next->prev = header;
        }
    }
    mem->total_free += freed_size;
    return 0;
}

int qmem_init(QMem *mem, void *mempool, size_t size, size_t align, size_t min_split, uint8_t magic, void (*lock)(bool))
{
    if(!mem || !mempool) {
        return -1;
    }
    if(align == 0 || (align & (align - 1)) != 0) {
        return -1;
    }
    if(align < sizeof(void *)) {
        align = sizeof(void *);
    }

    uintptr_t base = align_up((uintptr_t)mempool, align);
    size_t adjust = base - (uintptr_t)mempool;
    if(size < adjust) {
        return -1;
    }
    size = (size - adjust) & ~(size_t)(align - 1);
    if(size < sizeof(QMemBlock)) {
        return -1;
    }
    if(min_split < sizeof(QMemBlock)) {
        min_split = sizeof(QMemBlock);
    }
    if(min_split > size) {
        min_split = size;
    }

    mem->align = align;
    mem->min_split = min_split;
    mem->magic = magic;
    mem->max_block = 0;
    mem->total_free = size;
    mem->blocks = (QMemBlock *)base;
    mem->pool_start = (void *)base;
    mem->pool_end = (void *)(base + size);
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
    void *ret = qmem_alloc_(mem, size);
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
        void *ret = qmem_alloc_(mem, size);
        if(mem->lock) {
            mem->lock(false);
        }
        return ret;
    }

    if(size == 0) {
        qmem_free_(mem, ptr);
        if(mem->lock) {
            mem->lock(false);
        }
        return NULL;
    }

    QMemBlock *header = block_from_ptr(mem, ptr);
    if(!header) {
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
        // Shrink: split and return excess to free list
        if((header->size >= total_size + mem->min_split) &&
           ((header->size - total_size) >= sizeof(QMemBlock))) {
            block_split(header, total_size, mem->magic);
            mem->total_free += header->next->size;
        }
        if(mem->lock) {
            mem->lock(false);
        }
        return ptr;
    }

    // Allocate new block without re-acquiring lock
    void *new_ptr = qmem_alloc_(mem, size);
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
    memcpy_(new_ptr, ptr, copy_size);

    // Free old block without re-acquiring lock
    qmem_free_(mem, ptr);

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
    int ret = qmem_free_(mem, ptr);
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
    /* recalculate max_block after merging */
    mem->max_block = 0;
    current = mem->blocks;
    while(current) {
        if(!current->used && current->size > mem->max_block) {
            mem->max_block = current->size;
        }
        current = current->next;
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
    if(mem->lock) {
        mem->lock(true);
    }
    mem->max_block = 0;
    mem->total_free = 0;

    QMemBlock *current = mem->blocks;
    while(current) {
        if(!current->used) {
            mem->total_free += current->size;
            if(current->size > mem->max_block) {
                mem->max_block = current->size;
            }
        }
        current = current->next;
    }
    if(mem->lock) {
        mem->lock(false);
    }
    return 0;
}
