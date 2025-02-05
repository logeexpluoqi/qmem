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

static inline int block_valid(QMem *mem)
{
    return (mem->block.magic == mem->magic);
}

int qmem_init(QMem *mem, void *mempool, qsize_t size, qsize_t align, qsize_t min_split, uint8_t magic)
{
    if((mem == qnull) || (mempool == qnull)) {
        return -1;
    }
    mem->align = align;
    mem->min_split = min_split;
    mem->magic = magic;
    mem->max_used = 0;
    mem->total_free = size;
    mem->free_list = (QMemBlock *)mempool;
    mem->block.next = qnull;
    mem->block.prev = qnull;
    mem->block.size = size;
    mem->block.used = 0;
    mem->block.magic = magic;
    return 0;
}

void *qmem_alloc(QMem *mem, qsize_t size)
{
    if((mem == qnull) || (size == 0) || (!mem->free_list)) {
        return qnull;
    }
    qsize_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    QMemBlock *best = qnull, *current = mem->free_list;

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
        mem->free_list = best->next;
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
    if(!block_valid(header)) {
        return;
    }

    header->used = 0;

    while(header->prev && header->prev->used == 0) {
        QMemBlock *prev_block = header->prev;
        prev_block->size += header->size;
        prev_block->next = header->next;
        if(header->next) {
            header->next->prev = prev_block;
        }
        header = prev_block;
    }

    while(header->next && header->next->used == 0) {
        QMemBlock *next_block = header->next;
        header->size += next_block->size;
        header->next = next_block->next;
        if(next_block->next) {
            next_block->next->prev = header;
        }
    }

    header->next = mem->free_list;
    header->prev = qnull;
    if(mem->free_list) {
        mem->free_list->prev = header;
    }
    mem->free_list = header;
}

int qmem_defrag(QMem *mem)
{
    if(mem == qnull) {
        return -1;
    }
    QMemBlock *current = mem->free_list;
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
