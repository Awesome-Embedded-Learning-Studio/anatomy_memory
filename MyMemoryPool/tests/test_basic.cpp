// 基本正确性测试（批 1）
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "memory_pool.hpp"

#include <cstring>
#include <vector>

// === alloc / free 基本正确性 ===

TEST_CASE("alloc/free round-trip 不崩") {
  void *p = MemoryPool::alloc(64);
  REQUIRE(p != nullptr);
  std::memset(p, 0xAB, 64);
  MemoryPool::free(p, 64);
}

TEST_CASE("alloc(0) 返 nullptr（v1 未处理的边界）") {
  CHECK(MemoryPool::alloc(0) == nullptr);
}

TEST_CASE("free(nullptr, ...) 安全") {
  MemoryPool::free(nullptr, 64); // 不应崩
  MemoryPool::free(nullptr, 0);
}

// === size class 边界 ===

TEST_CASE("size class 边界：1 / 16 / 17 / 127 / 128 都能分配且可写") {
  for (size_t sz : {1UL, 16UL, 17UL, 127UL, 128UL}) {
    void *p = MemoryPool::alloc(sz);
    REQUIRE(p != nullptr);
    std::memset(p, 0x5A, sz);
    MemoryPool::free(p, sz);
  }
}

TEST_CASE("大对象（>128B）走 operator new 且可写") {
  void *p = MemoryPool::alloc(4096);
  REQUIRE(p != nullptr);
  std::memset(p, 0xCD, 4096);
  MemoryPool::free(p, 4096);
}

// === make / destroy 对象 ===

struct Counted {
  int x;
  double y;
  Counted(int a = 0, double b = 0.0) : x(a), y(b) {}
};

TEST_CASE("make/destroy 构造析构，字段正确") {
  Counted *obj = MemoryPool::make<Counted>(42, 3.14);
  REQUIRE(obj != nullptr);
  CHECK(obj->x == 42);
  CHECK(obj->y == 3.14);
  MemoryPool::destroy(obj);
}

TEST_CASE("destroy(nullptr) 安全") {
  MemoryPool::destroy<Counted>(nullptr); // 不应崩
}

// === operator== 契约（无状态 allocator，return true 符合 STL 契约）===

TEST_CASE("PoolAllocator operator==：无状态，所有实例相等") {
  PoolAllocator<int> a1, a2;
  PoolAllocator<double> b;
  CHECK(a1 == a2);    // 同类型
  CHECK(a1 == b);     // 跨类型（底层都走全局 MemoryPool）
  CHECK(!(a1 != a2));
  CHECK(!(a1 != b));
}

// === PoolAllocator 配 STL 容器 ===

TEST_CASE("vector<int, PoolAllocator<int>> 基本操作") {
  std::vector<int, PoolAllocator<int>> v;
  for (int i = 0; i < 1000; ++i) v.push_back(i);
  REQUIRE(v.size() == 1000);
  CHECK(v[500] == 500);
}

TEST_CASE("vector 用 PoolAllocator 跨多个 size class 增长") {
  std::vector<char, PoolAllocator<char>> v;
  v.reserve(16);
  for (int i = 0; i < 4096; ++i) v.push_back(static_cast<char>(i));
  REQUIRE(v.size() == 4096);
  CHECK(v[2048] == static_cast<char>(2048));
}
