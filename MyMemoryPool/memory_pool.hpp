#pragma once

// 1. FreeList
// [meta data] -> [] -> []
// [data] -> [] -> []
#include "memory_pool_config.h"
#include <cstddef>
#include <mutex>
#include <new>
#include <utility>
struct FreeNode {
  FreeNode *next;
};

class FreeList {
public:
  // push
  // [] -> [] -> [] -> [] -> []
  void push(FreeNode *node) {
    node->next = head;
    head = node;
  }
  // pop
  FreeNode *pop() {
    if (!head) {
      return head;
    }

    FreeNode *node = head;
    head = node->next;
    return node;
  }

  // empty
  bool empty() const { return head == nullptr; }

private:
  FreeNode *head; // O(1)
};

// malloc
// free

class CentralPool {
public:
  // 找中心池要内存：空了向 OS 要，不再返 nullptr（v1 的"名不副实"已修）
  FreeNode *fetch(std::size_t size) {
    std::lock_guard<std::mutex> lock(mutex_); // RAII
    auto &list = pool_[idx(size)];
    if (!list.empty()) {
      return list.pop();
    }
    // 空了向 OS 要一个。正确性优先；批量切大块（Span/Page）留后续深耕。
    return reinterpret_cast<FreeNode *>(::operator new(size));
  }

  void release(size_t size, FreeNode *node) {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_[idx(size)].push(node);
  }


private:
  static constexpr size_t kMaxSmallSize = _kMaxSmallSize; // 128B
  static constexpr size_t kClassGrid = _kClassGrid;
  static constexpr size_t kNumClasses = kMaxSmallSize / kClassGrid;

  FreeList pool_[kNumClasses];
  std::mutex mutex_;

  static size_t idx(size_t sz) {
    size_t aligned = (sz + kClassGrid - 1) / kClassGrid;
    if (aligned == 0) {
      aligned = 1;
    }
    return aligned - 1;
  }
};

class ThreadCache {
public:
  FreeNode *alloc(size_t size, CentralPool &centralPool) {
    auto &list = freelists_[idx(size)];
    if (!list.empty()) {
      return list.pop();
    }

    // 缺料时向 CentralPool 批量索取（CentralPool 自己会向 OS 要，不返 nullptr）
    for (size_t i = 0; i < _kFetchTime; i++) {
      list.push(centralPool.fetch(size));
    }

    return list.pop();
  }

  void free(size_t size, FreeNode *node) { freelists_[idx(size)].push(node); }

  // 析构时把缓存归还 CentralPool，消除 TLS 跨线程泄漏（v1 的泄漏已修）
  ~ThreadCache();

private:
  static constexpr size_t kMaxSmallSize = _kMaxSmallSize; // 128B
  static constexpr size_t kClassGrid = _kClassGrid;
  static constexpr size_t kNumClasses = kMaxSmallSize / kClassGrid;

  FreeList freelists_[kNumClasses];
  // 注：原 mutex_ 是死代码（声明了从不加锁）——线程安全靠 thread_local，
  // 不是靠这把锁。已删除；若将来改锁策略再补。

  static size_t idx(size_t sz) {
    size_t aligned = (sz + kClassGrid - 1) / kClassGrid;
    if (aligned == 0) {
      aligned = 1;
    }
    return aligned - 1;
  }
};

class MemoryPool {
  friend class ThreadCache; // ThreadCache 析构需访问 central_pool 归还

public:
  static inline constexpr size_t alignGrid(size_t n) {
    return (n + _kClassGrid - 1) & ~size_t(_kClassGrid - 1);
  }

  static void *alloc(size_t size) {
    if (size == 0) {
      return nullptr; // alloc(0) 返 nullptr，边界明确（v1 未处理）
    }
    size = alignGrid(size);

    if (size > _kMaxSmallSize) {
      return ::operator new(size);
    }

    return tls_cache().alloc(size, central_pool);
  }

  static void free(void *ptr, size_t size) {
    if (!ptr) {
      return;
    }

    size = alignGrid(size);

    if (size > _kMaxSmallSize) {
      ::operator delete(ptr);
      return;
    }

    tls_cache().free(size, reinterpret_cast<FreeNode *>(ptr));
  }

  // obj
  template <typename T, typename... Args> static T *make(Args &&...args) {
    void *mem = alloc(sizeof(T));
    if (!mem) {
      throw std::bad_alloc();
    }
    try {
      return new (mem) T(std::forward<Args>(args)...);
    } catch (...) {
      free(mem, sizeof(T));
      throw;
    }
  }

  template <typename T> static void destroy(T *obj) { // v1 的 destory typo 已修
    if (!obj)
      return;
    obj->~T();
    free(reinterpret_cast<void *>(obj), sizeof(T));
  }

private:
  static ThreadCache &tls_cache() {
    thread_local ThreadCache cache;
    return cache;
  }

  static CentralPool central_pool;
};

inline CentralPool MemoryPool::central_pool;

// ThreadCache 析构：把每个档位的缓存归还 CentralPool。
// 假设：线程退出时 CentralPool 仍存活。主线程退出场景的全局静态 / thread_local
// 析构顺序交错是已知限制（教学实现，不做过度防御）。
inline ThreadCache::~ThreadCache() {
  for (size_t i = 0; i < kNumClasses; i++) {
    size_t size = (i + 1) * kClassGrid;
    auto &list = freelists_[i];
    while (!list.empty()) {
      MemoryPool::central_pool.release(size, list.pop());
    }
  }
}

template <typename T> struct PoolAllocator {
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template <typename U> struct rebind {
    using other = PoolAllocator<U>;
  };

  PoolAllocator() noexcept {}
  template <typename U> PoolAllocator(const PoolAllocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    void *p = MemoryPool::alloc(n * sizeof(T));
    if (!p) {
      throw std::bad_alloc();
    }
    return static_cast<T *>(p);
  }

  void deallocate(T *p, std::size_t n) noexcept {
    MemoryPool::free(p, n * sizeof(T));
  }

  // operator==：PoolAllocator 是无状态 allocator（无成员、都走全局 MemoryPool），
  // 任何实例可互相释放对方内存，返回 true 符合 STL 契约（同 std::allocator）。
  // 跨线程 free 的语义问题不在 operator==，在 free(ptr,size) 的 size 语义，留 A3。
  template <typename U>
  bool operator==(const PoolAllocator<U> &) const noexcept {
    return true;
  }
  template <typename U>
  bool operator!=(const PoolAllocator<U> &) const noexcept {
    return false;
  }
};
