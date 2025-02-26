/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:28
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-26 15:26
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
    mem->blocks->next = QNULL;
    mem->blocks->prev = QNULL;
    mem->blocks->magic = magic;
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
    // Check if the memory management structure is valid, size is non-zero, and there are available blocks
    if(!mem || (size == 0) || !mem->blocks) {
        return QNULL;
    }
    // Calculate the total size needed, including the block header and alignment
    qsize_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    // Pointer to the best-fit block found so far
    QMemBlock *best = QNULL;
    // Pointer to the current block being examined
    QMemBlock *current = mem->blocks;

    // Traverse the list of blocks to find the best-fit block
    while(current) {
        // Check if the current block is free and large enough
        if(!current->used && current->size >= total_size) {
            // Update the best-fit block if this block is smaller than the previous best
            if(!best || current->size < best->size) {
                best = current;
            }
        }
        // Move to the next block
        current = current->next;
    }

    // If no suitable block is found, return QNULL
    if(!best) {
        return QNULL;
    }

    // If the best-fit block is large enough to split
    if(best->size >= total_size + mem->min_split) {
        // Create a new block for the remaining free space
        QMemBlock *new_block = (QMemBlock *)((uint8_t *)best + total_size);

        // Initialize the new block
        new_block->size = best->size - total_size;
        new_block->used = 0;
        new_block->magic = mem->magic;

        // Insert the new block into the list
        new_block->next = best->next;
        new_block->prev = best;
        if(best->next) {
            best->next->prev = new_block;
        }
        best->next = new_block;

        // Update the size of the best-fit block
        best->size = total_size;
    }

    // Remove the best-fit block from the free list
    if(best->prev) {
        best->prev->next = best->next;
    } else {
        mem->blocks = best->next;
    }

    if(best->next) {
        best->next->prev = best->prev;
    }

    // Mark the best-fit block as used
    best->used = 1;
    // Return the pointer to the allocated memory (skipping the block header)
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
    if (!mem) {
        return QNULL;
    }

    // If ptr is NULL, allocate a new block
    if (ptr == QNULL) {
        return qmem_alloc(mem, size);
    }

    // If size is 0, free the block
    if (size == 0) {
        qmem_free(mem, ptr);
        return QNULL;
    }

    // Get the header of the memory block
    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    // Check if the block is valid
    if (!block_valid(mem, header)) {
        return QNULL;
    }

    // Calculate the total size needed, including the header
    qsize_t total_size = align_up(size + sizeof(QMemBlock), mem->align);
    // If the original block is large enough, return the original pointer
    if (header->size >= total_size) {
        return ptr;
    }

    // If the original block is not large enough, allocate a new block
    void *new_ptr = qmem_alloc(mem, size);
    // Check if the allocation was successful
    if (new_ptr == QNULL) {
        return QNULL;
    }

    // Copy the data from the old block to the new block
    memcpy(new_ptr, ptr, header->size - sizeof(QMemBlock));

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
    // Check if the memory management structure or the pointer is invalid
    if (!mem || !ptr) {
        return -1;
    }

    // Get the header of the memory block
    QMemBlock *header = (QMemBlock *)((uint8_t *)ptr - sizeof(QMemBlock));
    // Check if the block is valid
    if (!block_valid(mem, header)) {
        return -1;
    }

    // Mark the block as free
    header->used = 0;

    // Find the correct position to insert the freed block in the free list
    QMemBlock *prev = QNULL;
    QMemBlock *curr = mem->blocks;
    while (curr && ((uint8_t *)curr < (uint8_t *)header)) {
        prev = curr;
        curr = curr->next;
    }

    // Insert the freed block into the free list
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

    // Merge with the previous free block if adjacent
    if (header->prev && ((uint8_t *)header->prev + header->prev->size == (uint8_t *)header)) {
        header->prev->size += header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
        header = header->prev;
    }

    // Merge with the next free block if adjacent
    if (header->next && ((uint8_t *)header + header->size == (uint8_t *)header->next)) {
        header->size += header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    // Return success
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
 