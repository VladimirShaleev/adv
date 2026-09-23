#include <gtest/gtest.h>
#include <utils/string-pool.hpp>

#include "finally.hpp"

using namespace std::string_view_literals;
using namespace utils;

TEST(StringPool, EmptyString) {
    constexpr auto expected = ""sv;

    StringPool pool;
    const auto string = pool.insert(""sv);
    const auto actual = pool[string];

    EXPECT_EQ(actual, expected);
    EXPECT_EQ(string.handle, 0u);
}

TEST(StringPool, BasicInsertionAndDedup) {
    StringPool pool;

    auto h1 = pool.insert("hello"sv);
    auto h2 = pool.insert("world"sv);
    auto h3 = pool.insert("hello"sv);

    EXPECT_EQ(pool[h1], "hello"sv);
    EXPECT_EQ(pool[h2], "world"sv);

    EXPECT_EQ(h1.handle, h3.handle);
    EXPECT_NE(h1.handle, h2.handle);
}

TEST(StringPool, LinearProbingCollision) {
    StringPool pool(100, 65536, 143);

    std::vector<vm_string_h> handles;
    handles.reserve(100);
    for (size_t i = 0; i < 100; ++i) {
        std::string str = "ident_" + std::to_string(i);
        handles.push_back(pool.insert(str));
    }
    auto h20 = pool.insert("ident_20"sv);

    for (size_t i = 0; i < handles.size(); ++i) {
        std::string expected = "ident_" + std::to_string(i);
        EXPECT_EQ(pool[handles[i]], expected);
    }
    EXPECT_EQ(handles[20].handle, h20.handle);
}

TEST(StringPool, BufferResize) {
    StringPool pool(1024, 16);

    auto h1 = pool.insert("very_long_string_that_exceeds_initial_buffer_capacity"sv);
    auto h2 = pool.insert("another_long_string_to_trigger_more_resizes"sv);

    EXPECT_EQ(pool[h1], "very_long_string_that_exceeds_initial_buffer_capacity"sv);
    EXPECT_EQ(pool[h2], "another_long_string_to_trigger_more_resizes"sv);
}

TEST(StringPool, SlotsResize) {
    StringPool pool(2, 65536);

    auto h1 = pool.insert("one"sv);
    auto h2 = pool.insert("two"sv);
    auto h3 = pool.insert("three"sv);
    auto h4 = pool.insert("four"sv);
    auto h5 = pool.insert("five"sv);

    EXPECT_EQ(pool[h1], "one"sv);
    EXPECT_EQ(pool[h2], "two"sv);
    EXPECT_EQ(pool[h3], "three"sv);
    EXPECT_EQ(pool[h4], "four"sv);
    EXPECT_EQ(pool[h5], "five"sv);

    auto h6 = pool.insert("three"sv);
    EXPECT_EQ(h3.handle, h6.handle);
}

TEST(StringPool, HashTableResize) {
    StringPool pool(1024, 65536, 4);

    auto h1 = pool.insert("first_string"sv);
    EXPECT_EQ(pool[h1], "first_string"sv);

    auto h2 = pool.insert("second_string"sv);
    EXPECT_EQ(pool[h2], "second_string"sv);

    auto h3 = pool.insert("third_string"sv);
    EXPECT_EQ(pool[h3], "third_string"sv);

    auto h1Dup = pool.insert("first_string"sv);
    auto h2Dup = pool.insert("second_string"sv);
    auto h3Dup = pool.insert("third_string"sv);

    EXPECT_EQ(h1.handle, h1Dup.handle);
    EXPECT_EQ(h2.handle, h2Dup.handle);
    EXPECT_EQ(h3.handle, h3Dup.handle);
}

TEST(StringPool, AccessorsEquivalent) {
    StringPool pool;
    auto handle = pool.insert("test_string"sv);

    EXPECT_EQ(pool.get(handle), "test_string"sv);
    EXPECT_EQ(pool[handle], "test_string"sv);
}
