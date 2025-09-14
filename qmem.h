/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:29
 * @ Modified by: luoqi
 * @ Modified time: 2025-06-28 22:04
 * @ Description:
 */

#ifndef _QMEM_H
#define _QMEM_H
 
#include <stdint.h>
#include <stddef.h>
 
typedef struct qmem_t {
    size_t size;
    struct qmem_t *next;
    struct qmem_t *prev;
    uint8_t used;
    uint8_t magic;
} QMemBlock;
 
 
typedef struct {
    QMemBlock *blocks;
    size_t min_split;
    size_t align;
    size_t total_free;
    size_t max_block;
    uint8_t magic;
    void (*lock)(void);
    void (*unlock)(void);
} QMem;
 
int qmem_init(QMem *mem, void *mempool, size_t size, size_t align, size_t min_split, uint8_t magic, void (*lock)(void), void (*unlock)(void));
 
void *qmem_alloc(QMem *mem, size_t size);
 
void *qmem_realloc(QMem *mem, void *ptr, size_t size);

int qmem_free(QMem *mem, void *ptr);
 
int qmem_defrag(QMem *mem);

int qmem_status(QMem *mem);
 
#endif
 
 