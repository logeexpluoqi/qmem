/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:28
 * @ Modified by: luoqi
 * @ Modified time: 2025-03-28 17:34
 * @ Description:
 */

#include "qmem.h"

static inline qsize_t _align_up(qsize_t size, qsize_t align)
{
    if (align == 0) {
        align = 1;
    }
    return (size + align - 1) & ~(align - 1);
}

static inline int _block_valid(QMem *mem, QMemBlock *block)
{
    return (block->magic == mem->magic);
}

static inline void *_memcpy(void *dest, const void *src, qsize_t n)
{
    if (!dest || !src) {
        return QNULL;
    }

    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    while (n >= sizeof(uint64_t)) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += sizeof(uint64_t);
        s += sizeof(uint64_t);
        n -= sizeof(uint64_t);
    }

    while (n >= sizeof(uint32_t)) {
        *(uint32_t *)d = *(const uint32_t *)s;
        d += sizeof(uint32_t);
        s += sizeof(uint32_t);
        n -= sizeof(uint32_t);
    }

    while (n > 0) {
        *d++ = *s++;
        n--;
    }

    return dest;
}
int qmem_init(QMem *mem, void *mempool, qsize_t size, qsize_t align, qsize_t min_split, uint8_t magic, int (*lock)(void), int (*unlock)(void))
{
    if(!mem || !mempool || ((align & (align - 1)) != 0)) {
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
    mem->lock = lock;
    mem->unlock = unlock;
    return 0;
}

/**
 * @brief Allocate a block of memory from the given memory pool.
 *
 * This function searches for a suitable free block in the memory pool
 * managed by 'mem' to allocate a block of size 'size'. It uses the best-fit
 * algorithm to find the smallest free block that can accommodate the requested size.
 * If the found block is significantly larger than the requested size, it may split
 * the block into two parts: one for the allocation and one for the remaining free space.
 *
 * @param mem Pointer to the memory management structure.
 * @param size The size of the memory block to allocate.
 * @return A pointer to the allocated memory block, or QNULL if the allocation fails.
 */
void *qmem_alloc(QMem *mem, qsize_t size)
{
    if (!mem || (size == 0) || !mem->blocks) {
        return QNULL;
    }
    if (mem->lock) {
        mem->lock();
    }

    qsize_t total_size = _align_up(size + sizeof(QMemBlock), mem->align);
    if (total_size > mem->total_free) {
        if (mem->unlock) {
            mem->unlock();
        }
        return QNULL;
    }

    QMemBlock *best = QNULL;
    QMemBlock *current = mem->blocks;

    while (current) {
        if (!current->used && current->size >= total_size) {
            if (!best || current->size < best->size) {
                best = current;
            }
        }
        current = current->next;
    }

    if (!best) {
        if (mem->unlock) {
            mem->unlock();
        }
        return QNULL;
    }

    if (best->size >= total_size + mem->min_split) {
        QMemBlock *new_block = (QMemBlock *)((uint8_t *)best + total_size);
        new_block->size = best->size - total_size;
        new_block->used = 0;
        new_block->magic = mem->magic;

        new_block->next = best->next;
        new_block->prev = best;
        if (best->next) {
            best->next->prev = new_block;
        }
        best->next = new_block;

        best->size = total_size;
    }

    best->used = 1;
    mem->total_free -= best->size;

    if (mem->unlock) {
        mem->unlock();
    }
    return (void *)((uint8_t *)best + sizeof(QMemBlock));
}

/**
 * @brief Reallocate a memory block.
 *
 * This function attempts to resize the memory block pointed to by 'ptr' to the specified 'size'.
 * If 'ptr' is NULL, it behaves like qmem_alloc. If 'size' is 0, it behaves like qmem_free.
 *
 * @param mem Pointer to the memory management structure.
 * @param ptr Pointer to the memory block to be reallocated.
 * @param size The new size of the memory block.
 * @return Pointer to the reallocated memory block, or NULL if the allocation fails.
 */
void *qmem_realloc(QMem *mem, void *ptr, qsize_t size)
{
    // Check if the memory management structure is valid
    if (!mem || !ptr) {
        return QNULL;
    }
    if(mem->lock) {
        mem->lock();
    }

    // If ptr is NULL, allocate a new block
    if (ptr == QNULL) {
        return qmem_alloc(mem, size);
    }

    // If size is 0, free the block
    if (size == 0) {
        qmem_free(mem, ptr);
        if(mem->unlock) {
            mem->unlock();
        }
        return QNULL;
    }

    // Get the header of the memory block
    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    // Check if the block is valid
    if (!_block_valid(mem, header)) {
        if(mem->unlock) {
            mem->unlock();
        }
        return QNULL;
    }

    // Calculate the total size needed, including the header
    qsize_t total_size = _align_up(size + sizeof(QMemBlock), mem->align);
    // If the original block is large enough, return the original pointer
    if (header->size >= total_size) {
        if(mem->unlock) {
            mem->unlock();
        }
        return ptr;
    }

    // If the original block is not large enough, allocate a new block
    void *new_ptr = qmem_alloc(mem, size);
    // Check if the allocation was successful
    if (new_ptr == QNULL) {
        if(mem->unlock) {
            mem->unlock();
        }
        return QNULL;
    }

    // Copy the data from the old block to the new block
    _memcpy(new_ptr, ptr, header->size - sizeof(QMemBlock));

    // Free the original block
    qmem_free(mem, ptr);

    if(mem->unlock) {
        mem->unlock();
    }

    return new_ptr;
}

/**
 * @brief Free a previously allocated memory block.
 *
 * This function marks the specified memory block as free and inserts it back
 * into the free list. It also attempts to merge adjacent free blocks to reduce
 * fragmentation.
 *
 * @param mem Pointer to the memory management structure.
 * @param ptr Pointer to the memory block to be freed.
 * @return 0 on success, -1 on failure (e.g., invalid arguments or block).
 */
int qmem_free(QMem *mem, void *ptr)
{
    if (!mem || !ptr) {
        return -1;
    }
    if (mem->lock) {
        mem->lock();
    }

    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    if (!_block_valid(mem, header)) {
        if (mem->unlock) {
            mem->unlock();
        }
        return -1;
    }

    header->used = 0;
    mem->total_free += header->size;

    // 合并前后的空闲块
    if (header->prev && !header->prev->used) {
        header->prev->size += header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    }

    if (header->next && !header->next->used) {
        header->size += header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    if (mem->unlock) {
        mem->unlock();
    }
    return 0;
}

int qmem_defrag(QMem *mem)
{
    if (!mem) {
        return -1;
    }
    if (mem->lock) {
        mem->lock();
    }

    QMemBlock *current = mem->blocks;
    while (current && current->next) {
        if (!current->used && !current->next->used) {
            current->size += current->next->size;
            current->next = current->next->next;
            if (current->next) {
                current->next->prev = current;
            }
        } else {
            current = current->next;
        }
    }

    if (mem->unlock) {
        mem->unlock();
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
 