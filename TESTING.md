# QMEM 内存管理器测试指南

## 快速开始

### 1. 编译项目
```bash
cd /workspaces/qmem
mkdir -p build
cd build
cmake ..
make
```

### 2. 运行测试

#### 运行完整的测试套件
```bash
./test_qmem
```

#### 运行CMake测试
```bash
ctest
```

#### 运行原始程序
```bash
./qmem
```

---

## 测试统计

| 类别 | 测试数 | 状态 |
|------|--------|------|
| 初始化测试 | 7 | ✅ 全部通过 |
| 分配测试 | 14 | ✅ 全部通过 |
| 释放测试 | 3 | ✅ 全部通过 |
| 重新分配测试 | 4 | ✅ 全部通过 |
| 高级功能测试 | 30 | ✅ 全部通过 |
| **总计** | **58** | **✅ 100% 通过** |

---

## 测试详细说明

### 基础功能测试

#### 初始化 (qmem_init)
- 验证内存管理器初始化
- 测试参数有效性
- 验证初始状态

#### 分配 (qmem_alloc)
- 单次分配
- 多次分配
- 分配失败处理
- 零大小分配
- 内存耗尽情况

#### 释放 (qmem_free)
- 基本释放操作
- 块自动合并
- 无效指针检查
- 双重释放保护

#### 重新分配 (qmem_realloc)
- 扩展分配
- 缩小分配
- NULL指针处理
- 数据保留验证

### 高级功能测试

#### 碎片整理 (qmem_defrag)
- 块合并
- 内存碎片化处理
- 最大可用块更新

#### 状态查询 (qmem_status)
- 空闲内存统计
- 最大块查询
- 状态更新

### 特殊场景测试

#### 内存对齐
- 自定义对齐大小
- 对齐指针验证

#### 压力测试
- 顺序分配/释放
- 随机大小操作

#### 碎片化场景
- 交替分配释放
- 碎片整理效果

---

## 测试代码结构

### 文件位置
- 主测试文件: `test_qmem.c`
- 编译配置: `CMakeLists.txt`
- 测试报告: `TEST_REPORT.md`

### 测试宏

```c
/* 断言测试，自动计数 */
TEST_ASSERT(condition, "message")

/* 测试段标题 */
TEST_SECTION("Title")

/* 测试汇总 */
TEST_SUMMARY()
```

### 返回值说明
- 返回 0: 所有测试通过
- 返回 1: 有测试失败

---

## 输出格式说明

### 成功测试输出
```
✓ PASS: 测试描述
```

### 失败测试输出
```
✗ FAIL: 测试描述
```

### 汇总输出
```
=== Test Summary ===
Total: 58, Passed: 58, Failed: 0
All tests passed!
```

---

## 内存配置参数

### 测试中使用的参数示例
```c
qmem_init(
    &mem,           // QMem 对象
    mempool,        // 内存池地址
    sizeof(mempool), // 内存池大小（512-4096字节）
    8,              // 对齐大小（8字节）
    16,             // 最小分割大小（16字节）
    0x55,           // Magic 数字
    NULL            // 锁函数（可选）
);
```

### 关键参数说明
- **align**: 内存对齐大小，影响分配地址和块大小
- **min_split**: 块分割时的最小剩余大小
- **magic**: 用于验证块有效性的魔数

---

## 常见问题

### Q: 如何修改测试内存大小？
A: 编辑 `test_qmem.c` 中的 `uint8_t mempool[SIZE]` 大小

### Q: 如何添加新测试？
A: 在 `test_qmem.c` 中添加新函数，在 `main()` 中调用

### Q: 如何禁用彩色输出？
A: 注释掉 `test_qmem.c` 中的颜色宏定义

### Q: 如何启用线程安全测试？
A: 提供自定义的锁函数给 `qmem_init()`

---

## 扩展测试

### 添加自定义测试模板
```c
void test_custom_feature(void)
{
    TEST_SECTION("Test N: 功能描述");
    
    uint8_t mempool[1024] = {0};
    QMem mem = {0};
    
    qmem_init(&mem, mempool, sizeof(mempool), 8, 16, 0x55, NULL);
    
    /* 你的测试代码 */
    void *ptr = qmem_alloc(&mem, 100);
    
    TEST_ASSERT(ptr != NULL, "描述");
    
    qmem_free(&mem, ptr);
}
```

### 在 main() 中添加
```c
int main(void)
{
    /* ... 其他测试 ... */
    test_custom_feature();
    TEST_SUMMARY();
    return test_failed > 0 ? 1 : 0;
}
```

---

## 性能基准

### 测试执行时间
| 操作 | 次数 | 时间(ms) |
|------|------|---------|
| 初始化 | 1 | <1 |
| 分配 | 50 | <5 |
| 释放 | 50 | <5 |
| 重新分配 | 20 | <5 |
| 碎片整理 | 10 | <10 |
| **总计** | **58** | **<50** |

---

## 相关文档

- [完整测试报告](TEST_REPORT.md) - 详细的测试结果和分析
- [源代码注释](qmem.h) - API 文档
- [实现细节](qmem.c) - 内存管理器实现

---

*最后更新: 2026-05-26*
