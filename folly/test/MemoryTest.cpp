/*
 * Copyright 2015 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/Memory.h>
#include <folly/Arena.h>
#include <folly/String.h>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <type_traits>

using namespace folly;

namespace {
class disposable {
 public:
  explicit disposable(std::function<void()> onDispose) :
    onDispose_(std::move(onDispose)) {}
  static void dispose(disposable* f) {
    ASSERT_NE(nullptr, f);
    f->onDispose_();
    delete f;
  }
 private:
  std::function<void()> onDispose_;
};
}

TEST(static_function_deleter, example) {
  size_t count = 0;
  using disposable_deleter =
    static_function_deleter<disposable, &disposable::dispose>;
  make_unique<disposable, disposable_deleter>([&] { ++count; });
  EXPECT_EQ(1, count);
}

TEST(static_function_deleter, nullptr) {
  using disposable_deleter =
    static_function_deleter<disposable, &disposable::dispose>;
  std::unique_ptr<disposable, disposable_deleter>(nullptr);
}

TEST(to_shared_ptr, example) {
  auto uptr = make_unique<std::string>("hello");
  auto sptr = to_shared_ptr(std::move(uptr));
  EXPECT_EQ(nullptr, uptr);
  EXPECT_EQ("hello", *sptr);
}

TEST(to_shared_ptr, example_with_dtor) {
  bool disposed = false;
  using disposable_deleter =
    static_function_deleter<disposable, &disposable::dispose>;
  auto uptr =
    make_unique<disposable, disposable_deleter>([&] { disposed = true; });
  EXPECT_FALSE(disposed);
  auto sptr = to_shared_ptr(std::move(uptr));
  EXPECT_FALSE(disposed);
  sptr = nullptr;
  EXPECT_TRUE(disposed);
}

template <std::size_t> struct T {};
template <std::size_t> struct S {};
template <std::size_t> struct P {};

TEST(as_stl_allocator, sanity_check) {
  typedef StlAllocator<SysArena, int> stl_arena_alloc;

  EXPECT_TRUE((std::is_same<
    as_stl_allocator<int, SysArena>::type,
    stl_arena_alloc
  >::value));

  EXPECT_TRUE((std::is_same<
    as_stl_allocator<int, stl_arena_alloc>::type,
    stl_arena_alloc
  >::value));
}

TEST(StlAllocator, void_allocator) {
  typedef StlAllocator<SysArena, void> void_allocator;
  SysArena arena;
  void_allocator valloc(&arena);

  typedef void_allocator::rebind<int>::other int_allocator;
  int_allocator ialloc(valloc);

  auto i = std::allocate_shared<int>(ialloc, 10);
  ASSERT_NE(nullptr, i.get());
  EXPECT_EQ(10, *i);
  i.reset();
  ASSERT_EQ(nullptr, i.get());
}

TEST(rebind_allocator, sanity_check) {
  std::allocator<long> alloc;

  auto i = std::allocate_shared<int>(
    rebind_allocator<int, decltype(alloc)>(alloc), 10
  );
  ASSERT_NE(nullptr, i.get());
  EXPECT_EQ(10, *i);
  i.reset();
  ASSERT_EQ(nullptr, i.get());

  auto d = std::allocate_shared<double>(
    rebind_allocator<double>(alloc), 5.6
  );
  ASSERT_NE(nullptr, d.get());
  EXPECT_EQ(5.6, *d);
  d.reset();
  ASSERT_EQ(nullptr, d.get());

  auto s = std::allocate_shared<std::string>(
    rebind_allocator<std::string>(alloc), "HELLO, WORLD"
  );
  ASSERT_NE(nullptr, s.get());
  EXPECT_EQ("HELLO, WORLD", *s);
  s.reset();
  ASSERT_EQ(nullptr, s.get());
}

int main(int argc, char **argv) {
  FLAGS_logtostderr = true;
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
