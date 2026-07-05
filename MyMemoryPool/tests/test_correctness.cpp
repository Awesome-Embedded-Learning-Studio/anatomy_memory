// 正确性测试（批 2）：CentralPool 向 OS 要 / TLS 析构归还 / 跨线程 / ASan
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "memory_pool.hpp"

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

// === CentralPool 名副其实：fetch 不返 nullptr ===

TEST_CASE("高频分配（不 free）全部非空——CentralPool 真的向 OS 要了内存") {
  constexpr int N = 10000;
  std::vector<void *> ptrs;
  ptrs.reserve(N);
  for (int i = 0; i < N; ++i) {
    void *p = MemoryPool::alloc(64);
    REQUIRE(p != nullptr);
    std::memset(p, 0x5A, 64);
    ptrs.push_back(p);
  }
  for (void *p : ptrs) MemoryPool::free(p, 64);
}

// === 跨线程 alloc / free（A 线程分，B 线程放）===

TEST_CASE("跨线程：A alloc / B free 不崩，ASan 验证") {
  constexpr int kRounds = 1000;
  std::vector<void *> ptrs;
  ptrs.reserve(kRounds);

  std::thread a([&] {
    for (int i = 0; i < kRounds; ++i) {
      ptrs.push_back(MemoryPool::alloc(48));
    }
  });
  a.join();

  std::thread b([&] {
    for (void *p : ptrs) {
      std::memset(p, 0x11, 48); // 验证内存可用
      MemoryPool::free(p, 48);
    }
  });
  b.join();
}

// === 多线程并发 alloc/free（ASan 压力）===

TEST_CASE("8 线程并发 alloc/free 无 UB，ASan 验证") {
  constexpr int kThreads = 8;
  constexpr int kPerThread = 2000;

  std::vector<std::thread> threads;
  std::atomic<long> total{0};

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < kPerThread; ++i) {
        void *p = MemoryPool::alloc(32);
        std::memset(p, 0, 32);
        MemoryPool::free(p, 32);
        ++total;
      }
    });
  }
  for (auto &t : threads) t.join();

  CHECK(total.load() == kThreads * kPerThread);
}

// === ThreadCache 析构归还 CentralPool（消除 TLS 泄漏）===
// 启动若干线程做 alloc 不 free（强制 TLS 填充），线程退出时
// ~ThreadCache 应把缓存归还 CentralPool。ASan 不应报泄漏。

TEST_CASE("线程退出：ThreadCache 析构归还 TLS 剩余，无泄漏") {
  constexpr int kThreads = 4;
  for (int round = 0; round < 3; ++round) {
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&] {
        // alloc + free 配对：用户侧不泄漏。
        // TLS 在 alloc 时填充（fetch 一批）、free 时回收；线程退出时
        // ~ThreadCache 应把 TLS 剩余归还 CentralPool（节点经全局 central_pool
        // 可达，ASan 不报）。若 ~ThreadCache 没工作，TLS 节点随 thread_local
        // 销毁丢失指针 = ASan 报泄漏。
        for (int i = 0; i < 100; ++i) {
          void *p = MemoryPool::alloc(64);
          std::memset(p, 0x77, 64);
          MemoryPool::free(p, 64);
        }
      });
    }
    for (auto &t : threads) t.join();
  }
}

// === 多线程混合 size class（防串档，ASan 检测越界）===

TEST_CASE("多线程混合 size class 无串档，ASan 验证") {
  constexpr int kThreads = 4;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t] {
      const size_t sizes[] = {16, 32, 64, 96, 128};
      for (int i = 0; i < 500; ++i) {
        size_t sz = sizes[(i + t) % 5];
        void *p = MemoryPool::alloc(sz);
        std::memset(p, t, sz); // 写满整个分配，ASan 检测越界
        MemoryPool::free(p, sz);
      }
    });
  }
  for (auto &t : threads) t.join();
}
