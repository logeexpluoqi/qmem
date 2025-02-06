/**
 * @ Author: luoqi
 * @ Create Time: 2025-02-05 20:29
 * @ Modified by: luoqi
 * @ Modified time: 2025-02-05 22:14
 * @ Description:
 */

#ifndef _QMEM_H
#define _QMEM_H

#include <stdint.h>

#ifndef qsize_t
typedef uint32_t qsize_t;
#endif

#ifndef qnull
#define qnull ((void *)0)
#endif

typedef struct qmem_t {
    qsize_t size;
    struct qmem_t *next;
    struct qmem_t *prev;
    uint8_t used;
    uint8_t magic;
} QMemBlock;

typedef struct {
    QMemBlock *blocks;
    qsize_t min_split;
    qsize_t align;
    qsize_t total_free;
    qsize_t max_block;
    uint8_t magic;
} QMem;


int qmem_init(QMem *mem, void *mempool, qsize_t size, qsize_t align, qsize_t min_split, uint8_t magic);

void *qmem_alloc(QMem *mem, qsize_t size);

int qmem_free(QMem *mem, void *ptr);

int qmem_defrag(QMem *mem);

int qmem_status(QMem *mem);

#endif
