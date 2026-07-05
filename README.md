# anatomy_memory

> **从 malloc 到内存池：手写一个高性能分配器**

本项目配套视频教程：[内存池实战 - B站](https://space.bilibili.com/294645890/lists/7045956)

---

## 📺 视频教程

| # | 视频标题 | B站链接 | 状态 |
|---|----------|---------|------|
| 1 | 从内存池是什么出发 | [📺](https://www.bilibili.com/video/BV14g6TBcEyL/) | ✅ |
| 2 | malloc咋不行了？（上） | [📺](https://www.bilibili.com/video/BV1ES6hBQE1E/) | ✅ |
| 3 | malloc咋不行了？（下） | [📺](https://www.bilibili.com/video/BV1hN6aB6EU9/) | ✅ |
| 4 | 内存池之从FreeList入手 | [📺](https://www.bilibili.com/video/BV1aicGzgE1P/) | ✅ |
| 5 | 内存池之CentralPool | [📺](https://www.bilibili.com/video/BV1ehcGzCEdJ/) | ✅ |
| 6 | 内存池之ThreadLocal | [📺](https://www.bilibili.com/video/BV1ehcGzCEg7/) | ✅ |
| 7 | 内存池之内存池本尊 | [📺](https://www.bilibili.com/video/BV1v8cGz5EYj/) | ✅ |
| 8 | 内存池之benchmark试试味道 | [📺](https://www.bilibili.com/video/BV1qscGzmEoU/) | ✅ |
| 9 | 内存池之适配标准库（完结撒花） | [📺](https://www.bilibili.com/video/BV1v8cGz5Ecs/) | ✅ |

---

## 📚 学习路线

```
┌─────────────────────────────────────────────────────────┐
│                   MemoryPool 学习路线                    │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Step 1: 基础概念                                        │
│  ├── 从内存池是什么出发                                 │
│  └── 理解内存池的应用场景                               │
│                                                         │
│  Step 2: 问题分析                                        │
│  ├── malloc咋不行了？（上）                             │
│  ├── malloc咋不行了？（下）                             │
│  └── 理解通用分配器的性能瓶颈                           │
│                                                         │
│  Step 3: 数据结构设计                                    │
│  ├── 内存池之从FreeList入手                             │
│  └── 理解自由列表的设计                                 │
│                                                         │
│  Step 4: 核心组件实现                                    │
│  ├── 内存池之CentralPool                                │
│  ├── 内存池之ThreadLocal                                │
│  └── 内存池之内存池本尊                                 │
│                                                         │
│  Step 5: 性能测试与集成                                  │
│  ├── 内存池之benchmark试试味道                          │
│  └── 内存池之适配标准库（完结撒花）                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 🗂️ 项目结构

```
anatomy_memory/
├── MyMemoryPool/              # 内存池实现
│   ├── memory_pool.hpp        # 核心实现（单头文件）
│   ├── memory_pool_config.h   # 配置参数
│   ├── benchmark_config.h     # Benchmark 配置
│   ├── benchmark.cpp          # 性能测试程序
│   ├── notes.md               # 开发笔记
│   └── CMakeLists.txt         # 构建配置
│
├── native_malloc_test/         # 原生 malloc 测试对比
│   ├── frequent_malloc.c      # 频繁分配测试
│   ├── malloc_overhead.c      # 开销测试
│   ├── malloc_vs_pool.c       # 对比测试
│   ├── malloc_multithread.c   # 多线程测试
│   └── analyze_malloc.sh      # 分析脚本
│
└── README.md                  # 本文件
```

---

## 🚀 快速开始

### 环境要求

- **编译器**: 支持 C++17 的 GCC 8+ / Clang 7+
- **系统**: 推荐使用 WSL 或 Linux
- **构建工具**: CMake 3.10+

### 构建项目

```bash
# 进入内存池目录
cd MyMemoryPool

# 配置构建
cmake -B build -S .

# 编译（使用多线程加速）
cd build
make -j$(nproc)

# 运行 benchmark
./benchmark
```

### 运行测试

```bash
# 原生 malloc 测试
cd ../native_malloc_test

# 编译测试程序
gcc -o frequent_malloc frequent_malloc.c
gcc -o malloc_vs_pool malloc_vs_pool.c -pthread

# 运行对比测试
./malloc_vs_pool
```

---

## 🏗️ 内存池架构

```
┌─────────────────────────────────────────────┐
│           Application Code                  │
└─────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────┐
│            ThreadCache (thread_local)       │
│  每个线程独立的缓存，无锁分配/释放            │
└─────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────┐
│            CentralPool (全局共享)            │
│  管理多个 Size Class，线程安全                │
└─────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────┐
│               System Memory                 │
│        ::operator new / delete              │
└─────────────────────────────────────────────┘
```

### Size Classes

| 档位索引 | 大小范围 | 实际分配 |
|---------|----------|----------|
| 0 | 1-16 | 16 |
| 1 | 17-32 | 32 |
| 2 | 33-48 | 48 |
| 3 | 49-64 | 64 |
| 4 | 65-80 | 80 |
| 5 | 81-96 | 96 |
| 6 | 97-112 | 112 |
| 7 | 113-128 | 128 |

---

## 📖 配套文档

- **主仓库**: [CXXBaseComponents](https://github.com/your-username/Project_CXXBaseComponents)
- **教程文档**: [documentation/tutorial/memory_pool/](https://github.com/your-username/Project_CXXBaseComponents/tree/main/documentation/tutorial/memory_pool)
- **开发笔记**: [MyMemoryPool/notes.md](./MyMemoryPool/notes.md)

---

## 📊 性能对比

| 场景 | 系统 malloc | MemoryPool | 加速比 |
|------|------------|-----------|--------|
| 单线程 16B | 152 us | 45 us | ~3.3x |
| 单线程 32B | 149 us | 43 us | ~3.5x |
| 4线程 32B | 312 us | 44 us | ~7.1x |

*数据来源于 benchmark.cpp，实际结果因硬件而异*

---

## 👨‍💻 作者

- **作者**: Charliechen114514
- **B站**: [是的一个城管](https://space.bilibili.com/294645890)
- **组织**: [Awesome-Embedded-Learning-Studio](https://github.com/Awesome-Embedded-Learning-Studio)

---

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件
