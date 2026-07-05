# anatomy_memory

一个正在持续建设、专注于内存专题的教程——逐层解剖内存管理的内部原理。

**当前篇章**：用 C++ 手写一个三层内存分配器（FreeList → ThreadCache → CentralPool），从 `malloc` 在小对象高频分配下的代价出发，最终替换标准库的 `::operator new`。结构参考 tcmalloc 的前两层，用最少的代码讲清楚每个设计决策，不追求 mimalloc 那样的工业级特性。

配套 B 站《内存池实战》9 期视频：[合集](https://space.bilibili.com/294645890/lists/7045956)

[开发笔记](./MyMemoryPool/notes.md) · [Issue 反馈](https://github.com/Awesome-Embedded-Learning-Studio/anatomy_memory/issues)

---

## 为什么要有这个项目

`malloc` / `::operator new` 是通用分配器——它要服务所有大小、所有线程、所有访问模式，所以做了大量妥协。当你只分配小对象、只在多线程里高频申请释放时，这些妥协就成了开销：

- 每次分配都可能进全局锁（或级联锁）
- 小对象也带元数据开销
- 缓存局部性差，对象散布在堆各处

这个项目把"通用"拆掉，针对**小对象 + 多线程**重新设计一条快路径。重点不是造一个能投入生产的分配器，而是把每个设计决策讲清楚：为什么用 FreeList、为什么 ThreadCache 要 `thread_local`、为什么 CentralPool 要按 size class 分桶。

---

## 配套视频

9 期，从"为什么要造内存池"讲到"替换标准库"：

| # | 标题 | 链接 |
|---|------|------|
| 1 | 从内存池是什么出发 | [bilibili](https://www.bilibili.com/video/BV14g6TBcEyL/) |
| 2 | malloc 咋不行了？（上） | [bilibili](https://www.bilibili.com/video/BV1ES6hBQE1E/) |
| 3 | malloc 咋不行了？（下） | [bilibili](https://www.bilibili.com/video/BV1hN6aB6EU9/) |
| 4 | 内存池之从 FreeList 入手 | [bilibili](https://www.bilibili.com/video/BV1aicGzgE1P/) |
| 5 | 内存池之 CentralPool | [bilibili](https://www.bilibili.com/video/BV1ehcGzCEdJ/) |
| 6 | 内存池之 ThreadLocal | [bilibili](https://www.bilibili.com/video/BV1ehcGzCEg7/) |
| 7 | 内存池之内存池本尊 | [bilibili](https://www.bilibili.com/video/BV1v8cGz5EYj/) |
| 8 | 内存池之 benchmark 试试味道 | [bilibili](https://www.bilibili.com/video/BV1qscGzmEoU/) |
| 9 | 内存池之适配标准库 | [bilibili](https://www.bilibili.com/video/BV1v8cGz5Ecs/) |

---

## 怎么用

单头文件，include 即可。三组接口：

```cpp
#include "memory_pool.hpp"

// 1. 原始内存：分配 / 释放（自动对齐到 16B，走 thread_local 快路径）
void *p = MemoryPool::alloc(64);
MemoryPool::free(p, 64);

// 2. 对象：构造 / 析构
Foo *obj = MemoryPool::make<Foo>(arg1, arg2);
MemoryPool::destory(obj);

// 3. STL 容器：换掉默认分配器
std::vector<int, PoolAllocator<int>> v{1, 2, 3};
```

`alloc` 会把请求大小向上对齐到 16B；超过 128B 的大对象直接走 `::operator new`，不过三层池。

---

## 架构

三层结构，对应三种分配速度——绝大多数分配应该在最上面的 ThreadCache 命中：

```
应用代码
   │   命中 ThreadCache → 无锁，O(1)
   ▼
ThreadCache   （每线程一个，thread_local）
   │   缺料时向 CentralPool 批量索取
   ▼
CentralPool   （全局共享，按 size class 分桶）
   │   缺料时向 OS 切大块
   ▼
::operator new / delete
```

- **ThreadCache**：每个线程独占一份，分配和释放在自己的缓存里走完，不进任何锁。这是快路径，设计目标是让热分配尽量在这里命中。
- **CentralPool**：线程间共享，按 size class 分桶管理空闲块。ThreadCache 缺料时向它批量索取，锁竞争被"批量"摊薄。
- **OS 兜底**：CentralPool 也没料时，向系统申请大块内存再切分。

### Size class

小对象按 16 字节步进分档（对齐到 16），同档对象挂在同一个 FreeList 上，分配退化成 pop 一个节点：

| 档位 | 大小范围 | 实际分配 |
|------|----------|----------|
| 0 | 1 – 16 B | 16 B |
| 1 | 17 – 32 B | 32 B |
| 2 | 33 – 48 B | 48 B |
| 3 | 49 – 64 B | 64 B |
| 4 | 65 – 80 B | 80 B |
| 5 | 81 – 96 B | 96 B |
| 6 | 97 – 112 B | 112 B |
| 7 | 113 – 128 B | 128 B |

---

## 项目结构

```
anatomy_memory/
├── MyMemoryPool/              分配器实现
│   ├── memory_pool.hpp        核心实现，单头文件约 200 行
│   ├── memory_pool_config.h   可调参数
│   ├── benchmark_config.h     benchmark 配置
│   ├── benchmark.cpp          性能测试程序
│   ├── notes.md               设计取舍与踩坑笔记
│   └── CMakeLists.txt
│
└── native_malloc_test/         原生 malloc 的对比演示
    ├── frequent_malloc.c       频繁分配
    ├── malloc_overhead.c       开销对比
    ├── malloc_vs_pool.c        池 vs malloc
    ├── malloc_multithread.c    多线程
    └── analyze_malloc.sh
```

---

## 构建

工具链（代码用了 C++23 的 `<print>`）：

- GCC 14+ 或 Clang 18+（或同等版本）
- CMake 3.20+
- Linux / WSL

```bash
cd MyMemoryPool
cmake -B build -S .
cmake --build build -j

./build/benchmark               # 跑 benchmark
```

跑原生 malloc 的对比演示：

```bash
cd native_malloc_test
gcc -o malloc_vs_pool malloc_vs_pool.c -pthread
./malloc_vs_pool
```

---

## 性能

多线程高频小对象场景下，本池相对系统 `malloc`（ptmalloc）有显著加速——主要来自 `thread_local` 的 ThreadCache 消除了热路径上的锁竞争。

> 当前 `native_malloc_test/` 是教学演示，不是严格的基准测试（无 median/p99、无多基线）。严谨测量见下方 Roadmap。

---

## Roadmap

**已完成**（v1，配套 9 期视频）：

- 三层池：FreeList / ThreadCache / CentralPool
- `thread_local` 快路径，分配走无锁路径
- size class 分桶（16B 步进，覆盖到 128B）
- STL 适配（`PoolAllocator`）

**后续方向**：

- 严格的 benchmark：median / p99 / 多基线（mimalloc、tcmalloc）
- CentralPool 增加 Span / Page 层，向 OS 批量切内存
- ThreadCache 生命周期与 CentralPool 的归还策略
- 工业级分配器（mimalloc）源码导读，作为对比终章

---

## 作者

Charliechen114514 · B 站 [是的一个城管](https://space.bilibili.com/294645890) · [Awesome-Embedded-Learning-Studio](https://github.com/Awesome-Embedded-Learning-Studio)

MIT License，详见 [LICENSE](LICENSE)。
