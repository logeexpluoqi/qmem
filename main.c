/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:29
 * @ Modified by: luoqi
 * @ Modified time: 2025-06-28 22:05
 * @ Description:
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "qmem.h"

static uint8_t mempool[1024 * 4] = { 0 };

static QMem mem = { 0 };

void mem_stats(void)
{
    qmem_status(&mem);
    printf("Free memory: %lu, Largest block: %lu\r\n", (unsigned long)mem.total_free, (unsigned long)mem.max_block);
}

int main()
{
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    mem_stats();
    int *arr1 = qmem_alloc(&mem, 100 * sizeof(int));
    if(arr1 == NULL) {
        printf(" #! arr1 alloc failed \r\n");
    }
    mem_stats();
    char *str = qmem_alloc(&mem, 1280);
    if(str == NULL) {
        printf(" #! str alloc failed \r\n");
    }
    mem_stats();
    int *arr2 = qmem_alloc(&mem, 50 * sizeof(int));
    if(arr2 == NULL) {
        printf(" #! arr2 alloc failed \r\n");
    }
    mem_stats();


    qmem_free(&mem, arr1);
    mem_stats();
    qmem_free(&mem, str);
    mem_stats();

    // 这里应该可以合并arr1和str释放的空间
    void *big = qmem_alloc(&mem, 200 * sizeof(int));  // 测试大块分配
    if(big == NULL) {
        printf(" #! big alloc failed \r\n");
    }
    mem_stats();

    qmem_free(&mem, arr2);
    mem_stats();
    qmem_free(&mem, big);
    mem_stats();

    void *big1 = qmem_alloc(&mem, 200 * sizeof(int));  // 测试大块分配
    if(big1 == NULL) {
        printf(" #! big1 alloc failed \r\n");
    }
    mem_stats();
    void *big2 = qmem_alloc(&mem, 200 * sizeof(int));  // 测试大块分配
    if(big2 == NULL) {
        printf(" #! big2 alloc failed \r\n");
    }
    mem_stats();

    qmem_free(&mem, big1);
    qmem_free(&mem, big2);

    mem_stats();

    return 0;
}