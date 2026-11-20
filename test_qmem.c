/**
 * @ Author: Test Suite Generator
 * @ Description: Comprehensive test cases for qmem memory manager
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "qmem.h"

/* Test counters */
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

/* Color codes for output */
#define GREEN   "\x1b[32m"
#define RED     "\x1b[31m"
#define YELLOW  "\x1b[33m"
#define RESET   "\x1b[0m"

/* Test macros */
#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            test_passed++; \
            printf(GREEN "✓ PASS" RESET ": %s\n", message); \
        } else { \
            test_failed++; \
            printf(RED "✗ FAIL" RESET ": %s\n", message); \
        } \
    } while(0)

#define TEST_SECTION(title) \
    printf("\n" YELLOW "=== %s ===" RESET "\n", title)

#define TEST_SUMMARY() \
    printf("\n" YELLOW "=== Test Summary ===" RESET "\n"); \
    printf("Total: %d, Passed: %d, Failed: %d\n", test_count, test_passed, test_failed); \
    if (test_failed == 0) { \
        printf(GREEN "All tests passed!" RESET "\n"); \
    } else { \
        printf(RED "Some tests failed!" RESET "\n"); \
    }

/* Test 1: Basic initialization */
void test_init_basic(void)
{
    TEST_SECTION("Test 1: Basic Initialization");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    int result = qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    TEST_ASSERT(result == 0, "Init should return 0 on success");
    TEST_ASSERT(mem.blocks != NULL, "Blocks should be initialized");
    TEST_ASSERT(mem.total_free <= sizeof(mempool), "Total free should fit within mempool size");
    TEST_ASSERT(mem.magic == 0x55, "Magic number should be set");
    TEST_ASSERT(mem.align == 8, "Alignment should be set");
}

/* Test 2: Invalid initialization */
void test_init_invalid(void)
{
    TEST_SECTION("Test 2: Invalid Initialization");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    int result = qmem_init(NULL, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    TEST_ASSERT(result != 0, "Init with NULL mem should fail");
    
    result = qmem_init(&mem, NULL, sizeof(mempool), 8, 16, 0x55, NULL);
    TEST_ASSERT(result != 0, "Init with NULL mempool should fail");

    result = qmem_init(&mem, mempool, sizeof(mempool), 3, 16, 0x55, NULL);
    TEST_ASSERT(result != 0, "Init with non-power-of-two alignment should fail");

    result = qmem_init(&mem, mempool, sizeof(QMemBlock) - 1, 8, 16, 0x55, NULL);
    TEST_ASSERT(result != 0, "Init with a pool smaller than a block header should fail");
}

/* Test 3: Basic allocation */
void test_alloc_basic(void)
{
    TEST_SECTION("Test 3: Basic Allocation");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr1 = qmem_alloc(&mem, 100);
    TEST_ASSERT(ptr1 != NULL, "Should allocate 100 bytes successfully");
    
    void *ptr2 = qmem_alloc(&mem, 200);
    TEST_ASSERT(ptr2 != NULL, "Should allocate another 200 bytes");
    
    TEST_ASSERT(ptr1 != ptr2, "Two allocations should have different addresses");
}

/* Test 4: Allocation with zero size */
void test_alloc_zero_size(void)
{
    TEST_SECTION("Test 4: Allocation with Zero Size");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 0);
    TEST_ASSERT(ptr == NULL, "Allocating 0 bytes should return NULL");
}

/* Test 5: Allocation failure when out of memory */
void test_alloc_out_of_memory(void)
{
    TEST_SECTION("Test 5: Allocation Out of Memory");
    
    uint8_t mempool[256] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr1 = qmem_alloc(&mem, 200);
    TEST_ASSERT(ptr1 != NULL, "First allocation should succeed");
    
    void *ptr2 = qmem_alloc(&mem, 100);
    TEST_ASSERT(ptr2 == NULL, "Second allocation should fail (out of memory)");
}

/* Test 6: Basic free */
void test_free_basic(void)
{
    TEST_SECTION("Test 6: Basic Free");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 100);
    int result = qmem_free(&mem, ptr);
    TEST_ASSERT(result == 0, "Free should return 0 on success");
}

/* Test 7: Free invalid pointer */
void test_free_invalid(void)
{
    TEST_SECTION("Test 7: Free Invalid Pointer");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    int result = qmem_free(&mem, NULL);
    TEST_ASSERT(result != 0, "Free NULL pointer should fail");
    
    /* Test with a pointer outside the mempool */
    uint8_t external_buffer[100];
    void *invalid_ptr = (void *)external_buffer;
    result = qmem_free(&mem, invalid_ptr);
    TEST_ASSERT(result != 0, "Free external pointer should fail");

    void *realloc_result = qmem_realloc(&mem, invalid_ptr, 32);
    TEST_ASSERT(realloc_result == NULL, "Realloc external pointer should fail");
}

/* Test 8: Allocate after free */
void test_alloc_after_free(void)
{
    TEST_SECTION("Test 8: Allocate After Free");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr1 = qmem_alloc(&mem, 100);
    qmem_free(&mem, ptr1);
    
    void *ptr2 = qmem_alloc(&mem, 100);
    TEST_ASSERT(ptr2 != NULL, "Should be able to allocate after freeing");
    TEST_ASSERT(ptr1 == ptr2, "Reused freed block should have same address");
}

/* Test 9: Block merging on free */
void test_free_block_merge(void)
{
    TEST_SECTION("Test 9: Block Merging on Free");
    
    uint8_t mempool[512] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr1 = qmem_alloc(&mem, 100);
    void *ptr2 = qmem_alloc(&mem, 100);
    void *ptr3 = qmem_alloc(&mem, 100);
    
    qmem_free(&mem, ptr1);
    qmem_free(&mem, ptr2);
    
    void *ptr_large = qmem_alloc(&mem, 200);
    TEST_ASSERT(ptr_large != NULL, "Should be able to allocate 200 bytes after merging freed blocks");
}

/* Test 10: Realloc with larger size */
void test_realloc_expand(void)
{
    TEST_SECTION("Test 10: Realloc Expand");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 100);
    TEST_ASSERT(ptr != NULL, "Initial allocation should succeed");
    
    /* Write some data */
    uint8_t *data = (uint8_t *)ptr;
    for (int i = 0; i < 100; i++) {
        data[i] = i % 256;
    }
    
    /* Realloc to larger size */
    void *new_ptr = qmem_realloc(&mem, ptr, 200);
    TEST_ASSERT(new_ptr != NULL, "Realloc to larger size should succeed");
    
    /* Verify data is preserved */
    data = (uint8_t *)new_ptr;
    int data_correct = 1;
    for (int i = 0; i < 100; i++) {
        if (data[i] != (i % 256)) {
            data_correct = 0;
            break;
        }
    }
    TEST_ASSERT(data_correct, "Data should be preserved after realloc");
}

/* Test 11: Realloc with smaller size */
void test_realloc_shrink(void)
{
    TEST_SECTION("Test 11: Realloc Shrink");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 200);
    TEST_ASSERT(ptr != NULL, "Initial allocation should succeed");
    
    /* Write some data */
    uint8_t *data = (uint8_t *)ptr;
    for (int i = 0; i < 100; i++) {
        data[i] = i % 256;
    }
    
    /* Realloc to smaller size */
    void *new_ptr = qmem_realloc(&mem, ptr, 100);
    TEST_ASSERT(new_ptr != NULL, "Realloc to smaller size should succeed");
    
    /* Verify data is preserved */
    data = (uint8_t *)new_ptr;
    int data_correct = 1;
    for (int i = 0; i < 100; i++) {
        if (data[i] != (i % 256)) {
            data_correct = 0;
            break;
        }
    }
    TEST_ASSERT(data_correct, "Data should be preserved after shrink realloc");
}

/* Test 12: Realloc with NULL pointer */
void test_realloc_null_ptr(void)
{
    TEST_SECTION("Test 12: Realloc with NULL Pointer");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_realloc(&mem, NULL, 100);
    TEST_ASSERT(ptr != NULL, "Realloc with NULL pointer should allocate new memory");
}

/* Test 13: Realloc with zero size */
void test_realloc_zero_size(void)
{
    TEST_SECTION("Test 13: Realloc with Zero Size");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 100);
    void *result = qmem_realloc(&mem, ptr, 0);
    TEST_ASSERT(result == NULL, "Realloc with size 0 should return NULL");
}

/* Test 14: Multiple allocations and frees */
void test_multiple_alloc_free(void)
{
    TEST_SECTION("Test 14: Multiple Allocations and Frees");
    
    uint8_t mempool[2048] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = qmem_alloc(&mem, 64);
        TEST_ASSERT(ptrs[i] != NULL, "Allocation should succeed");
    }
    
    for (int i = 0; i < 10; i++) {
        int result = qmem_free(&mem, ptrs[i]);
        TEST_ASSERT(result == 0, "Free should succeed");
    }
}

/* Test 15: Defragmentation */
void test_defragmentation(void)
{
    TEST_SECTION("Test 15: Defragmentation");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr1 = qmem_alloc(&mem, 100);
    void *ptr2 = qmem_alloc(&mem, 100);
    void *ptr3 = qmem_alloc(&mem, 100);
    
    qmem_free(&mem, ptr1);
    qmem_free(&mem, ptr3);
    
    int result = qmem_defrag(&mem);
    TEST_ASSERT(result == 0, "Defrag should return 0");
    
    /* After defrag, should be able to allocate larger block */
    void *large_ptr = qmem_alloc(&mem, 200);
    TEST_ASSERT(large_ptr != NULL, "Should allocate larger block after defrag");
}

/* Test 16: Status check */
void test_status_check(void)
{
    TEST_SECTION("Test 16: Status Check");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr1 = qmem_alloc(&mem, 200);
    void *ptr2 = qmem_alloc(&mem, 300);
    
    size_t total_free = 0;
    size_t max_block = 0;
    int result = qmem_status(&mem, &total_free, &max_block);
    TEST_ASSERT(result == 0, "Status should return 0");
    TEST_ASSERT(total_free > 0, "Total free should be greater than 0");
    TEST_ASSERT(max_block > 0, "Max block should be greater than 0");
    TEST_ASSERT(total_free == mem.total_free, "Total free output should match allocator state");
    TEST_ASSERT(max_block == mem.max_block, "Max block output should match allocator state");
}

/* Test 17: Alignment test */
void test_alignment(void)
{
    TEST_SECTION("Test 17: Alignment Test");
    
    uint8_t mempool[2048] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool + 1, sizeof(mempool) - 1, 16, 32, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 100);
    TEST_ASSERT(ptr != NULL, "Allocation should succeed with alignment");
    
    /* Check if pointer is aligned */
    uintptr_t addr = (uintptr_t)ptr;
    int is_aligned = (addr % 16) == 0;
    TEST_ASSERT(is_aligned, "Allocated pointer should be aligned");
}

/* Test 18: Stress test - sequential alloc/free */
void test_stress_sequential(void)
{
    TEST_SECTION("Test 18: Stress Test - Sequential");
    
    uint8_t mempool[4096] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    int success_count = 0;
    for (int i = 0; i < 50; i++) {
        void *ptr = qmem_alloc(&mem, 32 + (i % 32));
        if (ptr != NULL) {
            success_count++;
            qmem_free(&mem, ptr);
        }
    }
    TEST_ASSERT(success_count > 40, "Most allocations should succeed");
}

/* Test 19: Memory fragmentation scenario */
void test_fragmentation_scenario(void)
{
    TEST_SECTION("Test 19: Memory Fragmentation Scenario");
    
    uint8_t mempool[2048] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptrs[8];
    for (int i = 0; i < 8; i++) {
        ptrs[i] = qmem_alloc(&mem, 96);
    }
    
    /* Free every other block to create fragmentation */
    for (int i = 0; i < 8; i += 2) {
        qmem_free(&mem, ptrs[i]);
    }
    
    /* Try to allocate large block (should fail due to fragmentation) */
    void *large = qmem_alloc(&mem, 384);
    
    /* Defrag and try again */
    qmem_defrag(&mem);
    void *large_after_defrag = qmem_alloc(&mem, 384);
    TEST_ASSERT(large_after_defrag != NULL, "Should allocate large block after defrag");
}

/* Test 20: Double free protection */
void test_double_free_protection(void)
{
    TEST_SECTION("Test 20: Double Free Protection");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    void *ptr = qmem_alloc(&mem, 100);
    int result1 = qmem_free(&mem, ptr);
    int result2 = qmem_free(&mem, ptr);
    
    TEST_ASSERT(result1 == 0, "First free should succeed");
    TEST_ASSERT(result2 != 0, "Second free should fail");
}

/* Main test runner */
int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   QMEM Memory Manager Test Suite       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_init_basic();
    test_init_invalid();
    test_alloc_basic();
    test_alloc_zero_size();
    test_alloc_out_of_memory();
    test_free_basic();
    test_free_invalid();
    test_alloc_after_free();
    test_free_block_merge();
    test_realloc_expand();
    test_realloc_shrink();
    test_realloc_null_ptr();
    test_realloc_zero_size();
    test_multiple_alloc_free();
    test_defragmentation();
    test_status_check();
    test_alignment();
    test_stress_sequential();
    test_fragmentation_scenario();
    test_double_free_protection();
    
    TEST_SUMMARY();
    
    return test_failed > 0 ? 1 : 0;
}
