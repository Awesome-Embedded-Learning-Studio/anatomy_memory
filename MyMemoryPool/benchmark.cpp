#include "benchmark_config.h"
#include "memory_pool.hpp"
#include <chrono>
#include <cstddef>
#include <new>
#include <print>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

volatile void *global_sink = nullptr;
struct TestObj {
  int a;
  double b;
  char padding[32];

  TestObj(int x = 0, double y = 0.0) : a(x), b(y) { a += b; }
  ~TestObj() { global_sink = (void *)this; }
};

namespace {
void print_header(const char *s) { std::print("\n=== {} ===\n", s); }

template <typename CreateFn, typename DestroyFn>
void benchmark_st(const char *name, CreateFn create, DestroyFn destroy) {
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < kOps; i++) {
    void *p = create();
    global_sink = p;
    destroy(p);
  }
  auto end = std::chrono::high_resolution_clock::now();

  double duration = std::chrono::duration<double>(end - start).count();
  std::println("{}: {}s", name, duration);
}

template <typename CreateFn, typename DestroyFn>
void benchmark_mt(const char *name, CreateFn create, DestroyFn destroy) {

  auto worker = [&]() {
    for (size_t i = 0; i < kOps; i++) {
      void *p = create();
      global_sink = p;
      destroy(p);
    }
  };

  std::vector<std::thread> threads;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < kThreads; i++) {
    threads.emplace_back(worker);
  }

  for (auto &t : threads) {
    t.join();
  }

  auto end = std::chrono::high_resolution_clock::now();

  double duration = std::chrono::duration<double>(end - start).count();
  std::println("{}: {}s", name, duration);
}

void bench_vector_small(const char *name, size_t elements_per_vector) {
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < kOps / 100; i++) {
    std::vector<TestObj, PoolAllocator<TestObj>> v;
    for (size_t j = 0; j < elements_per_vector; j++) {
      v.emplace_back(static_cast<int>(j), j * 0.5);
      global_sink = &v;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();

  double duration = std::chrono::duration<double>(end - start).count();
  std::println("{}: {}s", name, duration);
}

void bench_vector_smallstd(const char *name, size_t elements_per_vector) {
  auto start = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < kOps / 100; i++) {
    std::vector<TestObj> v;
    for (size_t j = 0; j < elements_per_vector; j++) {
      v.emplace_back(static_cast<int>(j), j * 0.5);
      global_sink = &v;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();

  double duration = std::chrono::duration<double>(end - start).count();
  std::println("{}: {}s", name, duration);
}

} // namespace

// benchmark
int main() {
  print_header("Test Simple New/Delete, Malloc/Free");

  auto system_alloc = []() -> void * { return ::operator new(kSize); };
  auto system_free = [](void *p) -> void { ::operator delete(p); };

  auto pool_alloc = []() -> void * { return MemoryPool::alloc(kSize); };
  auto pool_free = [](void *p) -> void { MemoryPool::free(p, kSize); };

  benchmark_st("operator new/delete", system_alloc, system_free);
  benchmark_st("Pool new/delete", pool_alloc, pool_free);

  print_header("Test MultiThread");
  benchmark_mt("operator new/delete", system_alloc, system_free);
  benchmark_mt("Pool new/delete", pool_alloc, pool_free);

  print_header("Test SmallObj Single");
  benchmark_st(
      "Standard Small Obj Single",
      []() -> void * { return new TestObj(1, 2.0); },
      [](void *p) {
        TestObj *obj = reinterpret_cast<TestObj *>(p);
        delete obj;
      });

  benchmark_st(
      "Pool Small Obj Single",
      []() -> void * { return MemoryPool::make<TestObj>(1, 2.0); },
      [](void *p) {
        TestObj *obj = reinterpret_cast<TestObj *>(p);
        MemoryPool::destroy(obj);
      });

  print_header("Test SmallObj MultiThread");
  benchmark_mt(
      "Standard Small Obj Single",
      []() -> void * { return new TestObj(1, 2.0); },
      [](void *p) {
        TestObj *obj = reinterpret_cast<TestObj *>(p);
        delete obj;
      });

  benchmark_mt(
      "Pool Small Obj Single",
      []() -> void * { return MemoryPool::make<TestObj>(1, 2.0); },
      [](void *p) {
        TestObj *obj = reinterpret_cast<TestObj *>(p);
        MemoryPool::destroy(obj);
      });

  bench_vector_small("vector test 1", 1);
  bench_vector_smallstd("vector test 1", 1);

  bench_vector_small("vector test 2", 2);
  bench_vector_smallstd("vector test 2", 2);

  bench_vector_small("vector test 4", 4);
  bench_vector_smallstd("vector test 4", 4);
  return 0;
}
