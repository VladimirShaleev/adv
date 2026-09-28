#include <gtest/gtest.h>
#include <utils/pool.hpp>

#include <algorithm>
#include <vector>

namespace {

struct TestHandle {
    size_t handle;
};

struct TrivialValue {
    using Handle = TestHandle;

    int value;
};

struct TrackedValue {
    using Handle = TestHandle;

    static inline int constructors = 0;
    static inline std::vector<int> destroyedValues;

    int value = 0;

    TrackedValue() { ++constructors; }
    ~TrackedValue() { destroyedValues.push_back(value); }
};

} // namespace

using namespace utils;

TEST(Pool, InitialStateAndCapacityGrowth) {
    Pool<TrivialValue> pool(2);

    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.capacity(), 2u);

    const auto first = pool.obtain();
    const auto second = pool.obtain();
    const auto [third, thirdValue] = pool.obtainAndAccess();
    pool.access(first)->value = 10;
    pool.access(second)->value = 20;
    thirdValue->value = 30;

    EXPECT_FALSE(pool.empty());
    EXPECT_EQ(pool.size(), 3u);
    EXPECT_GE(pool.capacity(), 3u);
    EXPECT_EQ(first.handle, 0u);
    EXPECT_EQ(second.handle, 1u);
    EXPECT_EQ(third.handle, 2u);
    EXPECT_EQ(pool.access(first)->value, 10);
    EXPECT_EQ(pool.access(second)->value, 20);
    EXPECT_EQ(pool.access(third)->value, 30);
}

TEST(Pool, ReusesHandlesReleasedOutOfOrder) {
    Pool<TrivialValue> pool(4);
    const auto first = pool.obtain();
    const auto second = pool.obtain();
    const auto third = pool.obtain();
    const auto fourth = pool.obtain();
    pool.access(first)->value = 10;
    pool.access(second)->value = 20;
    pool.access(third)->value = 30;
    pool.access(fourth)->value = 40;

    pool.release(second);
    pool.release(fourth);

    const auto reusedFourth = pool.obtain();
    const auto reusedSecond = pool.obtain();

    EXPECT_EQ(reusedFourth.handle, fourth.handle);
    EXPECT_EQ(reusedSecond.handle, second.handle);
    EXPECT_EQ(pool.access(first)->value, 10);
    EXPECT_EQ(pool.access(third)->value, 30);
    EXPECT_EQ(pool.access(reusedFourth)->value, 0);
    EXPECT_EQ(pool.access(reusedSecond)->value, 0);
    EXPECT_EQ(pool.size(), 4u);
}

TEST(Pool, FindsHandleForObjectPointer) {
    Pool<TrivialValue> pool(2);
    const auto first = pool.obtain();
    const auto [second, secondValue] = pool.obtainAndAccess();

    const auto* constFirstValue = pool.access(first);
    const auto* constSecondValue = secondValue;
    EXPECT_EQ(pool.handle(pool.access(first)).handle, first.handle);
    EXPECT_EQ(pool.handle(secondValue).handle, second.handle);
    EXPECT_EQ(pool.handle(constFirstValue).handle, first.handle);
    EXPECT_EQ(pool.handle(constSecondValue).handle, second.handle);

    pool.release(first);
    EXPECT_EQ(pool.handle(constFirstValue).handle, Pool<TrivialValue>::Undefined.handle);
}

TEST(Pool, ConstructsAndDestroysOnlyLiveObjects) {
    TrackedValue::constructors = 0;
    TrackedValue::destroyedValues.clear();
    TrackedValue::destroyedValues.reserve(4);

    {
        Pool<TrackedValue> pool(4);
        const auto first = pool.obtain();
        const auto released = pool.obtain();
        const auto third = pool.obtain();
        const auto fourth = pool.obtain();
        pool.access(first)->value = 10;
        pool.access(released)->value = 20;
        pool.access(third)->value = 30;
        pool.access(fourth)->value = 40;

        EXPECT_EQ(TrackedValue::constructors, 4);
        pool.release(released);
        EXPECT_EQ(pool.size(), 3u);
        EXPECT_EQ(TrackedValue::destroyedValues, std::vector<int>{20});
    }

    EXPECT_EQ(TrackedValue::destroyedValues.size(), 4u);
    EXPECT_EQ(TrackedValue::destroyedValues, (std::vector<int>{20, 10, 30, 40}));
}
