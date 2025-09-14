/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:28
 * @ Modified by: luoqi
 * @ Modified time: 2025-06-28 22:04
 * @ Description:
 */

#include "qmem.h"

static inline size_t align_up(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

static inline int block_valid(QMem *mem, QMemBlock *block)
{
    if (!mem || !block) {
        return 0;
    }
    if (block->magic != mem->magic) {
        return 0;
    }
    if (block->size < sizeof(QMemBlock)) {
        return 0;
    }
    return 1;
}

static void *_memcpy(void *dest, const void *src, size_t size)
{
    if (!dest || !src || size == 0) {
        return dest;
    }
    
    uint8_t *d = dest;
    const uint8_t *s = src;
    for(size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

int qmem_init(QMem *mem, void *mempool, size_t size, size_t align, size_t min_split, uint8_t magic, void (*lock)(void), void (*unlock)(void))
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
 * @return A pointer to the allocated memory block, or NULL if the allocation fails.
 */
void *qmem_alloc(QMem *mem, size_t size)
{
    if(!mem || (size == 0) || !mem->blocks) {
        return NULL;
    }
    if(mem->lock) {
        mem->lock();
    }
    
    size_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    if (total_size < size || total_size < sizeof(QMemBlock)) {
        if(mem->unlock) {
            mem->unlock();
        }
        return NULL;
    }
    QMemBlock *current = mem->blocks;

    // first fit
    while(current) {
        // Check if the current block is free and large enough
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
            if(mem->unlock) {
                mem->unlock();
            }
            return (void *)((uint8_t *)current + sizeof(QMemBlock));
        }
        // Move to the next block
        current = current->next;
    }
    if(mem->unlock) {
        mem->unlock();
    }
    return NULL;
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
void *qmem_realloc(QMem *mem, void *ptr, size_t size)
{
    // Check if the memory management structure is valid
    if (!mem) {
        return NULL;
    }

    // If ptr is NULL, allocate a new block
    if (ptr == NULL) {
        return qmem_alloc(mem, size);
    }

    // If size is 0, free the block
    if (size == 0) {
        qmem_free(mem, ptr);
        return NULL;
    }

    // Get the header of the memory block
    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    // Check if the block is valid
    if (!block_valid(mem, header)) {
        return NULL;
    }

    // Calculate the total size needed, including the header
    size_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    // If the original block is large enough, return the original pointer
    if (header->size >= total_size) {
        return ptr;
    }

    // If the original block is not large enough, allocate a new block
    void *new_ptr = qmem_alloc(mem, size);
    // Check if the allocation was successful
    if (new_ptr == NULL) {
        return NULL;
    }

    // Copy the data from the old block to the new block
    _memcpy(new_ptr, ptr, header->size - sizeof(QMemBlock));

    // Free the original block
    qmem_free(mem, ptr);

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

    // Get the header of the memory block
    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    // Check if the block is valid
    if (!block_valid(mem, header)) {
        if(mem->unlock) {
            mem->unlock();
        }
        return -1;
    }

    // Mark the block as free
    header->used = 0;

    // merge with prev
    if (header->prev && !header->prev->used && 
        ((uint8_t *)header->prev + header->prev->size == (uint8_t *)header)) {
        header->prev->size += header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    }

    if (header->next && !header->next->used && 
        ((uint8_t *)header + header->size == (uint8_t *)header->next)) {
        header->size += header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }
    mem->total_free += header->size;
    if(mem->unlock) {
        mem->unlock();
    }
    return 0;
}

int qmem_defrag(QMem *mem)
{
    if(!mem) {
        return -1;
    }
    if(mem->lock) {
        mem->lock();
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
    if(mem->unlock) {
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
