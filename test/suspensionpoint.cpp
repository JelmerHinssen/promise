#include <gtest/gtest.h>

#include <array>

#include "promise.h"

using namespace promise;
using namespace std;

static auto& living = promise::Coroutine::living;

class SuspensionTest : public testing::Test {
   public:
    enum FunctionNames {
        SIMPLE_SUSPENSION_0,
        SIMPLE_SUSPENSION_1,
        INT_SUSPENSION_0,
        INT_SUSPENSION_1,
        INT_SUSPENSION_2,
        NESTED_SUSPENSION_0,
        NESTED_SUSPENSION_1,
        NESTED_SUSPENSION_2,
        DANGLING_0,
        DANGLING_1,
        DANGLING_2,
        DANGLING_NESTED_0,
        DANGLING_NESTED_1,
        FUNCTION_COUNT
    };

    SuspensionTest() { living.clear(); }
    ~SuspensionTest() { EXPECT_TRUE(living.empty()); }

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    promise::optional<int> result;

    SuspensionPoint<void> point;
    SuspensionPoint<int> int_point;
    Promise<void> simple_suspension() {
        function_counts[SIMPLE_SUSPENSION_0]++;
        co_await point;
        function_counts[SIMPLE_SUSPENSION_1]++;
    }
    Promise<void, int> int_suspension() {
        function_counts[INT_SUSPENSION_0]++;
        auto x = co_await int_point;
        function_counts[INT_SUSPENSION_1]++;
        co_yield x;
        function_counts[INT_SUSPENSION_2]++;
    }
    Promise<void, int> nested_suspension() {
        function_counts[NESTED_SUSPENSION_0]++;
        co_await int_suspension();
        function_counts[NESTED_SUSPENSION_1]++;
        co_yield 3;
        function_counts[NESTED_SUSPENSION_2]++;
    }
    Promise<void> dangling() {
        function_counts[DANGLING_0]++;
        co_await point;
        result = 1;
        function_counts[DANGLING_1]++;
        result = co_await int_point;
        function_counts[DANGLING_2]++;
    }
    Promise<void> dangling_nested() {
        function_counts[DANGLING_NESTED_0]++;
        co_await dangling();
        function_counts[DANGLING_NESTED_1]++;
    }
};

TEST_F(SuspensionTest, simpleSuspension) {
    auto p = simple_suspension();
    p->start();
    expected_counts[SIMPLE_SUSPENSION_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_FALSE(p->yielded());
    point.resume();
    expected_counts[SIMPLE_SUSPENSION_1]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->done());
    EXPECT_FALSE(p->yielded());
}

TEST_F(SuspensionTest, intSuspension) {
    auto p = int_suspension();
    p->start();
    expected_counts[INT_SUSPENSION_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_FALSE(p->yielded());
    int_point.resume(2);
    expected_counts[INT_SUSPENSION_1]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_TRUE(p->yielded());
    EXPECT_EQ(p->yielded_value(), 2);
    p->resume();
    expected_counts[INT_SUSPENSION_2]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->done());
    EXPECT_FALSE(p->yielded());
}

TEST_F(SuspensionTest, NestedSuspension) {
    auto p = nested_suspension();
    p->start();
    expected_counts[NESTED_SUSPENSION_0]++;
    expected_counts[INT_SUSPENSION_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_FALSE(p->yielded());

    int_point.resume(2);
    expected_counts[INT_SUSPENSION_1]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_TRUE(p->yielded());
    EXPECT_EQ(p->yielded_value(), 2);

    p->resume();
    expected_counts[INT_SUSPENSION_2]++;
    expected_counts[NESTED_SUSPENSION_1]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_TRUE(p->yielded());
    EXPECT_EQ(p->yielded_value(), 3);

    p->resume();
    expected_counts[NESTED_SUSPENSION_2]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->done());
    EXPECT_FALSE(p->yielded());
}
TEST_F(SuspensionTest, danglingFinished) {
    {
        auto p = dangling();
        p->start();
    }
    ASSERT_EQ(living.size(), 1);
    expected_counts[DANGLING_0]++;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);

    point.resume();
    ASSERT_EQ(living.size(), 1);
    expected_counts[DANGLING_1]++;
    EXPECT_EQ(result, 1);
    EXPECT_EQ(function_counts, expected_counts);

    int_point.resume(2);
    ASSERT_EQ(living.size(), 0);
    expected_counts[DANGLING_2]++;
    EXPECT_EQ(result, 2);
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(SuspensionTest, danglingUnfinished) {
    {
        auto p = dangling();
        p->start();
    }
    ASSERT_EQ(living.size(), 1);
    expected_counts[DANGLING_0]++;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);

    point.resume();
    ASSERT_EQ(living.size(), 1);
    expected_counts[DANGLING_1]++;
    EXPECT_EQ(result, 1);
    EXPECT_EQ(function_counts, expected_counts);
    int_point = {};
}

TEST_F(SuspensionTest, danglingNestedFinished) {
    {
        auto p = dangling_nested();
        p->start();
    }
    ASSERT_EQ(living.size(), 2);
    expected_counts[DANGLING_NESTED_0]++;
    expected_counts[DANGLING_0]++;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);

    point.resume();
    ASSERT_EQ(living.size(), 2);
    expected_counts[DANGLING_1]++;
    EXPECT_EQ(result, 1);
    EXPECT_EQ(function_counts, expected_counts);

    int_point.resume(3);
    ASSERT_EQ(living.size(), 0);
    expected_counts[DANGLING_2]++;
    expected_counts[DANGLING_NESTED_1]++;
    EXPECT_EQ(result, 3);
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(SuspensionTest, danglingNestedUnfinished) {
    {
        auto p = dangling_nested();
        p->start();
    }
    ASSERT_EQ(living.size(), 2);
    expected_counts[DANGLING_NESTED_0]++;
    expected_counts[DANGLING_0]++;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);

    point.resume();
    ASSERT_EQ(living.size(), 2);
    expected_counts[DANGLING_1]++;
    EXPECT_EQ(result, 1);
    EXPECT_EQ(function_counts, expected_counts);

    int_point = {};
}
