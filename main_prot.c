/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:29
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-05 22:19
 * @ Description:
 */

#include "qmem.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "qmem.h"

#define MEM_POOL_SIZE 4096        // 内存池大小
#define BLOCK_ALIGNMENT 8         // 对齐要求
#define MIN_SPLIT_SIZE 32         // 最小可分割剩余空间

#define QMEM_MAGIC (0xAA)

 // 内存块头结构（双向链表）
typedef struct mem_block_t {
    size_t size;                  // 包含头部的总大小
    struct mem_block_t *next;    // 下一个块
    struct mem_block_t *prev;    // 上一个块
    uint8_t used;                 // 使用标志
    uint8_t magic;                // 魔数校验
} mem_block_t;

static uint8_t memory_pool[MEM_POOL_SIZE];
static mem_block_t *free_list = NULL;

// 内存对齐计算
static inline size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

void mem_init(void)
{
    free_list = (mem_block_t *)memory_pool;
    free_list->size = MEM_POOL_SIZE;
    free_list->next = NULL;
    free_list->prev = NULL;
    free_list->used = 0;
    free_list->magic = QMEM_MAGIC;
}

// 块有效性验证
static int block_valid(mem_block_t *block)
{
    return block->magic == QMEM_MAGIC;
}

void *mem_alloc(size_t size)
{
    if(size == 0 || !free_list) return NULL;

    // 计算需要分配的总空间（包含头部和对齐）
    size_t total_size = align_up(size + sizeof(mem_block_t), BLOCK_ALIGNMENT);
    mem_block_t *best = NULL, *current = free_list;

    // 最佳适应算法：寻找最小足够大的块
    while(current) {
        if(!current->used && current->size >= total_size) {
            if(!best || current->size < best->size) {
                best = current;
            }
        }
        current = current->next;
    }

    if(!best) return NULL;

    // 检查是否需要分割块
    if(best->size >= total_size + MIN_SPLIT_SIZE) {
        mem_block_t *new_block = (mem_block_t *)((uint8_t *)best + total_size);

        // 初始化新块
        new_block->size = best->size - total_size;
        new_block->used = 0;
        new_block->magic = 0xAA;

        // 维护双向链表
        new_block->next = best->next;
        new_block->prev = best;
        if(best->next)
            best->next->prev = new_block;
        best->next = new_block;

        best->size = total_size;
    }

    // 从空闲链表移除
    if(best->prev)
        best->prev->next = best->next;
    else
        free_list = best->next;

    if(best->next)
        best->next->prev = best->prev;

    best->used = 1;
    return (void *)((uint8_t *)best + sizeof(mem_block_t));
}

void mem_free(void* ptr) {
    if (!ptr) return;

    mem_block_t* header = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    if (!block_valid(header)) return;

    header->used = 0;

    /* 前向合并（修复版）*/
    mem_block_t* current_prev;
    while ((current_prev = header->prev) != NULL && 
           current_prev->used == 0 && 
           (uint8_t*)current_prev + current_prev->size == (uint8_t*)header) {
        // 合并到前块
        current_prev->size += header->size;
        current_prev->next = header->next;
        if (header->next)
            header->next->prev = current_prev;
        header = current_prev; // 关键：更新当前头指针
    }

    /* 后向合并（修复版）*/
    mem_block_t* current_next;
    while ((current_next = header->next) != NULL && 
           current_next->used == 0 && 
           (uint8_t*)header + header->size == (uint8_t*)current_next) {
        // 合并到当前块
        header->size += current_next->size;
        header->next = current_next->next;
        if (current_next->next)
            current_next->next->prev = header;
    }

    /* 插入空闲链表 */
    header->next = free_list;
    header->prev = NULL;
    if (free_list)
        free_list->prev = header;
    free_list = header;
}

void mem_stats(void)
{
    mem_block_t *current = free_list;
    size_t total_free = 0, max_block = 0;

    while(current) {
        total_free += current->size;
        if(current->size > max_block) max_block = current->size;
        current = current->next;
    }
    printf("Free memory: %zu, Largest block: %zu\r\n", total_free, max_block);
}

void mem_defrag(void)
{
    mem_block_t *current = free_list;
    while(current && current->next) {
        if((uint8_t *)current + current->size == (uint8_t *)current->next) {
            current->size += current->next->size;
            current->next = current->next->next;
            if(current->next)
                current->next->prev = current;
        } else {
            current = current->next;
        }
    }
}

/* 使用示例 */
int main()
{
    mem_init();

    mem_stats();
    int *arr1 = mem_alloc(100 * sizeof(int));
    mem_stats();
    char *str = mem_alloc(128);
    mem_stats();
    int *arr2 = mem_alloc(50 * sizeof(int));
    mem_stats();


    mem_free(arr1);
    mem_stats();
    mem_free(str);
    mem_stats();

    // 这里应该可以合并arr1和str释放的空间
    void *big = mem_alloc(200 * sizeof(int));  // 测试大块分配
    mem_stats();

    mem_free(arr2);
    mem_stats();
    mem_free(big);
    mem_stats();

    return 0;
}