#include <gtest/gtest.h>
#include <array>
#include "promise.h"

using namespace promise;
using namespace std;

static auto& living = promise::Coroutine::living;

class PromiseTest : public testing::Test {
public:
    enum FunctionNames {
        EMPTY_CO,
        EMPTY_RETURNING,
        CONDITIONAL_RETURNING,
        YIELDING_CO_0,
        YIELDING_CO_1,
        NESTED_EMPTY_0,
        NESTED_EMPTY_1,
        NESTED_YIELDING_0,
        NESTED_YIELDING_1,
        NESTED_YIELDING_2,
        FUNCTION_COUNT
    };

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    PromiseTest() {
        living.clear();
    }
    ~PromiseTest() {
        EXPECT_TRUE(living.empty());
    }
    Promise<void> empty_co (){
        function_counts[EMPTY_CO]++;
        co_return;
    }
    Promise<int> empty_returning (){
        function_counts[EMPTY_RETURNING]++;
        co_return 1;
    }
    Promise<int> conditional_returning (bool b) {
        function_counts[CONDITIONAL_RETURNING]++;
        if (b) co_return 1;
    }
    Promise<void, int> yielding_co() {
        function_counts[YIELDING_CO_0]++;
        co_yield 5;
        function_counts[YIELDING_CO_1]++;
        co_return;
    }
    Promise<void> nested_empty() {
        function_counts[NESTED_EMPTY_0]++;
        co_await empty_co();
        function_counts[NESTED_EMPTY_1]++;
        co_return;
    }
    Promise<void, int> nested_yielding() {
        function_counts[NESTED_YIELDING_0]++;
        co_await yielding_co();
        function_counts[NESTED_YIELDING_1]++;
        co_await yielding_co();
        function_counts[NESTED_YIELDING_2]++;
        co_return;
    }
};

TEST_F(PromiseTest, emptyCoroutine) {
    auto p = empty_co();
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, emptyReturningCoroutine) {
    auto p = empty_returning();
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startEmptyReturningCoroutine) {
    auto p = empty_returning();
    p.start();
    expected_counts[EMPTY_RETURNING]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_EQ(p.return_value(), 1);
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startYieldingCoroutine) {
    auto p = yielding_co();
    p.start();
    expected_counts[YIELDING_CO_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_EQ(p.yield_value(), 5);
    EXPECT_TRUE(p.yielded());
}

TEST_F(PromiseTest, resumeYieldingCoroutine) {
    auto p = yielding_co();
    p.start();
    expected_counts[YIELDING_CO_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_EQ(p.yield_value(), 5);
    EXPECT_TRUE(p.yielded());
    p.resume();
    expected_counts[YIELDING_CO_1]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_TRUE(p.return_value());
    EXPECT_EQ(p.yield_value(), 5);
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startConditionalReturningCoroutine) {
    auto p = conditional_returning(false);
    p.start();
    expected_counts[CONDITIONAL_RETURNING]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startEmptyCoroutine) {
    auto p = empty_co();
    p.start();
    expected_counts[EMPTY_CO]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_TRUE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, nestedCoroutine) {
    auto p = nested_empty();
    p.start();
    expected_counts[NESTED_EMPTY_0]++;
    expected_counts[EMPTY_CO]++;
    expected_counts[NESTED_EMPTY_1]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_TRUE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, nestedYieldingCoroutine) {
    auto p = nested_yielding();
    p.start();
    expected_counts[NESTED_YIELDING_0]++;
    expected_counts[YIELDING_CO_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 2);
    EXPECT_TRUE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_EQ(p.yield_value(), 5);
    EXPECT_TRUE(p.yielded());

    p.resume();
    expected_counts[YIELDING_CO_1]++;
    expected_counts[NESTED_YIELDING_1]++;
    expected_counts[YIELDING_CO_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 2);
    EXPECT_TRUE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_EQ(p.yield_value(), 5);
    EXPECT_TRUE(p.yielded());

    p.resume();
    expected_counts[YIELDING_CO_1]++;
    expected_counts[NESTED_YIELDING_2]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_TRUE(p.return_value());
    EXPECT_EQ(p.yield_value(), 5); // ?
    EXPECT_FALSE(p.yielded());
}
