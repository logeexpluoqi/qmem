/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:28
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-17 14:56
 * @ Description:
 */

#include "qmem.h"

static inline qsize_t align_up(qsize_t size, qsize_t align)
{
    return (size + align - 1) & ~(align - 1);
}

static inline int block_valid(QMem *mem, QMemBlock *block)
{
    return (block->magic == mem->magic);
}

int qmem_init(QMem *mem, void *mempool, qsize_t size, qsize_t align, qsize_t min_split, uint8_t magic)
{
    if(!mem || !mempool || (size < sizeof(QMemBlock))) {
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
    mem->blocks->next = QNULL;
    mem->blocks->prev = QNULL;
    mem->blocks->magic = magic;
    return 0;
}

void *qmem_alloc(QMem *mem, qsize_t size)
{
    if(!mem || (size == 0) || !mem->blocks) {
        return QNULL;
    }
    qsize_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    QMemBlock *best = QNULL;
    QMemBlock *current = mem->blocks;

    while(current) {
        if(!current->used && current->size >= total_size) {
            if(!best || current->size < best->size) {
                best = current;
            }
        }
        current = current->next;
    }

    if(!best) {
        return QNULL;
    }

    if(best->size >= total_size + mem->min_split) {
        QMemBlock *new_block = (QMemBlock *)((uint8_t *)best + total_size);

        new_block->size = best->size - total_size;
        new_block->used = 0;
        new_block->magic = mem->magic;

        new_block->next = best->next;
        new_block->prev = best;
        if(best->next) {
            best->next->prev = new_block;
        }
        best->next = new_block;

        best->size = total_size;
    }

    if(best->prev) {
        best->prev->next = best->next;
    } else {
        mem->blocks = best->next;
    }

    if(best->next) {
        best->next->prev = best->prev;
    }

    best->used = 1;
    return (void *)((uint8_t *)best + sizeof(QMemBlock));
}

int qmem_free(QMem *mem, void *ptr)
{
    if (!mem || !ptr) {
        return -1;
    }

    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    if (!block_valid(mem, header)) {
        return -1;
    }

    header->used = 0;

    QMemBlock *prev = QNULL;
    QMemBlock *curr = mem->blocks;
    while (curr && ((uint8_t *)curr < (uint8_t *)header)) {
        prev = curr;
        curr = curr->next;
    }

    header->prev = prev;
    header->next = curr;
    if (prev) {
        prev->next = header;
    } else {
        mem->blocks = header;
    }
    if (curr) {
        curr->prev = header;
    }

    if (header->prev && ((uint8_t *)header->prev + header->prev->size == (uint8_t *)header)) {
        header->prev->size += header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    }

    if (header->next && ((uint8_t *)header + header->size == (uint8_t *)header->next)) {
        header->size += header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    return 0;
}

int qmem_defrag(QMem *mem)
{
    if(!mem) {
        return -1;
    }
    QMemBlock *current = mem->blocks;
    while(current && current->next) {
        if((uint8_t *)current + current->size == (uint8_t *)current->next) {
            current->size += current->next->size;
            current->next = current->next->next;
            if(current->next) {
                current->next->prev = current;
            }
        } else {
            current = current->next;
        }
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
