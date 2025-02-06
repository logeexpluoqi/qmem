/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:28
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-05 22:21
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
    if((mem == qnull) || (mempool == qnull)) {
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
    mem->blocks->next = qnull;
    mem->blocks->prev = qnull;
    mem->blocks->magic = magic;
    return 0;
}

void *qmem_alloc(QMem *mem, qsize_t size)
{
    if((mem == qnull) || (size == 0) || !mem->blocks) {
        return qnull;
    }
    qsize_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    QMemBlock *best = qnull;
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
        return qnull;
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
    if((mem == qnull) || !ptr) {
        return -1;
    }

    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    if(!block_valid(mem, header)) {
        return -1;
    }

    header->used = 0;

    QMemBlock *current_prev;
    while((current_prev = header->prev) != qnull &&
        current_prev->used == 0 &&
        (uint8_t *)current_prev + current_prev->size == (uint8_t *)header) {
        current_prev->size += header->size;
        current_prev->next = header->next;
        if(header->next) {
            header->next->prev = current_prev;
        }
        header = current_prev;
    }

    QMemBlock *current_next;
    while((current_next = header->next) != qnull &&
        current_next->used == 0 &&
        (uint8_t *)header + header->size == (uint8_t *)current_next) {
        header->size += current_next->size;
        header->next = current_next->next;
        if(current_next->next) {
            current_next->next->prev = header;
        }
    }

    header->next = mem->blocks;
    header->prev = qnull;
    if(mem->blocks) {
        mem->blocks->prev = header;
    }
    mem->blocks = header;
}

int qmem_defrag(QMem *mem)
{
    if(mem == qnull) {
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
}

int qmem_status(QMem *mem)
{
    if(mem == qnull) {
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
