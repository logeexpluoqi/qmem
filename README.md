# QMem

QMem 是一个面向 C 项目的固定内存池分配器。调用方提供一段连续内存，QMem 在这段内存内部维护分配块，不依赖系统堆来获取内存，适合需要限制动态内存来源或在嵌入式环境中使用的场景。

## 设计与实现

- **调用方管理内存池**：`qmem_init` 接收内存池地址和长度；初始化及后续分配均在该区域内操作。
- **块内元数据**：每个块的头部保存块总长度、使用状态、校验标记以及前后块链表指针。块长度包含头部和用户数据。
- **首次适配**：`qmem_alloc` 从块链表头开始查找第一个足够大的空闲块。剩余空间达到 `min_split` 且能容纳块头时才拆分。
- **相邻空闲块合并**：`qmem_free` 会尝试与前后相邻的空闲块合并；`qmem_defrag` 会遍历并合并相邻空闲块。
- **可选临界区钩子**：每个公开操作在访问分配器状态时调用 `lock(true)`，结束时调用 `lock(false)`。单线程环境可传 `NULL`。
- **可选内存复制实现**：默认使用 C 库的 `memcpy`。定义 `QMEM_LIBC_UNDEPENDENT` 后，`realloc` 使用内置复制实现。

分配与状态统计中的块大小包含块头，不等同于调用方可用的 payload 大小。对齐参数必须为 2 的幂（小于指针对齐时按指针对齐）；初始化会将内存池起始地址向上对齐，并将可用长度向下取整。传入的内存池应有足够空间容纳块头和实际分配。

## API

在 `qmem.h` 中声明：

```c
int qmem_init(
    QMem *mem,
    void *mempool,
    size_t size,
    size_t align,
    size_t min_split,
    uint8_t magic,
    void (*lock)(bool)
);

void *qmem_alloc(QMem *mem, size_t size);
void *qmem_realloc(QMem *mem, void *ptr, size_t size);
int qmem_free(QMem *mem, void *ptr);
int qmem_defrag(QMem *mem);
int qmem_status(QMem *mem, size_t *total_free, size_t *max_block);
```

| API | 说明 | 返回值 |
| --- | --- | --- |
| `qmem_init` | 初始化分配器和调用方提供的内存池。`align` 指定对齐要求；`min_split` 控制拆分时保留的最小剩余块阈值；`magic` 用作块有效性标记；`lock` 可为 `NULL`。 | 成功返回 `0`，参数无效返回 `-1`。 |
| `qmem_alloc` | 从内存池分配指定字节数。 | 成功返回指针；零字节、参数无效或空间不足时返回 `NULL`。 |
| `qmem_realloc` | 调整已有分配。`ptr == NULL` 时等同于分配；`size == 0` 时释放原块并返回 `NULL`。扩容时尽可能保留原数据；分配失败时返回 `NULL`。 | 成功返回新/原指针；失败返回 `NULL`。 |
| `qmem_free` | 释放由该分配器分配的块。 | 成功返回 `0`；空指针、无效指针或重复释放返回 `-1`。 |
| `qmem_defrag` | 合并链表中相邻的空闲块，并更新 `mem->max_block`。 | 成功返回 `0`；空分配器指针返回 `-1`。 |
| `qmem_status` | 重新计算空闲块统计。输出指针可为 `NULL`；同时更新 `mem->total_free` 和 `mem->max_block`。 | 成功返回 `0`；空分配器指针返回 `-1`。 |

`QMem` 公开结构中的 `total_free` 和 `max_block` 可直接读取，但如需刷新最新值，应调用 `qmem_status`。两项统计均按块总长度计量。

## 使用方法

```c
#include <stdint.h>
#include "qmem.h"

static uint8_t pool[4096];
static QMem mem;

int main(void)
{
    if(qmem_init(&mem, pool, sizeof(pool), 8, 16, 0x55, NULL) != 0) {
        return 1;
    }

    char *data = qmem_alloc(&mem, 128);
    if(data == NULL) {
        return 1;
    }

    /* 使用 data ... */

    size_t total_free;
    size_t max_block;
    if(qmem_status(&mem, &total_free, &max_block) == 0) {
        /* 查看当前空闲块总量和最大单块容量（均含块头） */
    }

    return qmem_free(&mem, data) != 0;
}
```

调用方应保证内存池在 `QMem` 使用期间持续有效，并只将由同一个分配器返回的指针传给 `qmem_free` 或 `qmem_realloc`。

## 构建与测试

项目使用 CMake 和 C11。可在仓库根目录执行：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

测试程序 `test_qmem` 覆盖初始化、分配与释放、无效/重复释放、重分配、合并、碎片场景、状态统计、对齐和顺序压力分配。
