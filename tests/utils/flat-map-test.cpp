#include <gtest/gtest.h>
#include <utils/flat-map.hpp>

namespace {

struct FlatMapTestHandle {
    size_t handle;
};

struct FlatMapTestValue {
    int value;
};

} // namespace

using namespace utils;

TEST(FlatMap, InsertsAndFindsValuesByKeyAndHandle) {
    FlatMap<int, FlatMapTestHandle, FlatMapTestValue> map;

    const auto [secondHandle, secondValue] = map.insert(20, {200});
    ASSERT_NE(secondValue, nullptr);
    EXPECT_EQ(secondValue->value, 200);

    const auto [firstHandle, firstValue] = map.insert(10, {100});
    ASSERT_NE(firstValue, nullptr);
    EXPECT_EQ(firstValue->value, 100);

    const auto [thirdHandle, thirdValue] = map.insert(30, {300});
    ASSERT_NE(thirdValue, nullptr);
    EXPECT_EQ(thirdValue->value, 300);

    EXPECT_NE(firstHandle, Undefined);
    EXPECT_NE(secondHandle, Undefined);
    EXPECT_NE(thirdHandle, Undefined);
    EXPECT_NE(firstHandle.handle, secondHandle.handle);
    EXPECT_NE(firstHandle.handle, thirdHandle.handle);
    EXPECT_NE(secondHandle.handle, thirdHandle.handle);

    const auto [foundFirstHandle, foundFirst] = map.find(10);
    const auto [foundSecondHandle, foundSecond] = map.find(20);
    const auto [foundThirdHandle, foundThird] = map.find(30);
    ASSERT_NE(foundFirst, nullptr);
    ASSERT_NE(foundSecond, nullptr);
    ASSERT_NE(foundThird, nullptr);
    EXPECT_EQ(foundFirstHandle.handle, firstHandle.handle);
    EXPECT_EQ(foundSecondHandle.handle, secondHandle.handle);
    EXPECT_EQ(foundThirdHandle.handle, thirdHandle.handle);
    EXPECT_EQ(foundFirst->value, 100);
    EXPECT_EQ(foundSecond->value, 200);
    EXPECT_EQ(foundThird->value, 300);

    EXPECT_EQ(map.get(firstHandle)->value, 100);
    EXPECT_EQ(map.get(secondHandle)->value, 200);
    EXPECT_EQ(map.get(thirdHandle)->value, 300);
}

TEST(FlatMap, DuplicateKeyOverrideValue) {
    FlatMap<int, FlatMapTestHandle, FlatMapTestValue> map;

    const auto [handle, value] = map.insert(42, {7});
    const auto [duplicateHandle, newValue] = map.insert(42, {15});

    EXPECT_EQ(duplicateHandle.handle, handle.handle);
    EXPECT_EQ(newValue, value);
    ASSERT_NE(newValue, nullptr);
    EXPECT_EQ(newValue->value, 15);
    EXPECT_EQ(map.get(handle)->value, 15);
}

TEST(FlatMap, EmplaceConstructsAndReplacesValue) {
    FlatMap<int, FlatMapTestHandle, FlatMapTestValue> map;

    const auto [handle, value] = map.emplace(42, 7);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->value, 7);

    const auto [replacementHandle, replacementValue] = map.emplace(42, 15);
    EXPECT_EQ(replacementHandle.handle, handle.handle);
    EXPECT_EQ(replacementValue, value);
    ASSERT_NE(replacementValue, nullptr);
    EXPECT_EQ(replacementValue->value, 15);
    EXPECT_EQ(map.get(handle)->value, 15);
}

TEST(FlatMap, MissingKeysAndInvalidHandlesReturnEmptyResults) {
    const FlatMap<int, FlatMapTestHandle, FlatMapTestValue> map;

    const auto [handle, value] = map.find(1);
    EXPECT_EQ(handle, Undefined);
    EXPECT_EQ(value, nullptr);
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(map.get(Undefined), nullptr);
    EXPECT_EQ(map.get(FlatMapTestHandle{1}), nullptr);

    FlatMap<int, FlatMapTestHandle, FlatMapTestValue> populated;
    const auto [validHandle, validValue] = populated.insert(10, {10});
    ASSERT_NE(validValue, nullptr);

    const auto [missingHandle, missingValue] = populated.find(5);
    EXPECT_EQ(missingHandle.handle, 0u);
    EXPECT_EQ(missingValue, nullptr);
    EXPECT_FALSE(populated.contains(5));
    EXPECT_TRUE(populated.contains(10));
    EXPECT_EQ(populated.get(FlatMapTestHandle{validHandle.handle + 1}), nullptr);
}

TEST(FlatMap, HandleAccessRemainsValidAfterMoreInsertions) {
    FlatMap<int, FlatMapTestHandle, FlatMapTestValue> map;

    const auto [middleHandle, middleValue] = map.insert(20, {200});
    ASSERT_NE(middleValue, nullptr);
    middleValue->value = 201;

    const auto [lastHandle, lastValue] = map.insert(30, {300});
    const auto [firstHandle, firstValue] = map.insert(10, {100});
    const auto [anotherHandle, anotherValue] = map.insert(25, {250});
    EXPECT_NE(lastHandle.handle, middleHandle.handle);
    EXPECT_NE(firstHandle.handle, middleHandle.handle);
    EXPECT_NE(anotherHandle.handle, middleHandle.handle);
    ASSERT_NE(lastValue, nullptr);
    ASSERT_NE(firstValue, nullptr);
    ASSERT_NE(anotherValue, nullptr);

    const auto* constMap = &map;
    const auto [foundHandle, foundValue] = constMap->find(20);
    ASSERT_NE(foundValue, nullptr);
    EXPECT_EQ(foundHandle.handle, middleHandle.handle);
    EXPECT_EQ(foundValue->value, 201);
    EXPECT_EQ(constMap->get(middleHandle)->value, 201);
}
