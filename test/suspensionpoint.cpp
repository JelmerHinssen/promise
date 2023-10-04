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
        MOVING_0,
        MOVING_1,
        NESTED_MOVING_0,
        NESTED_MOVING_1,
        FUNCTION_COUNT
    };

    SuspensionTest() { living.clear(); }
    ~SuspensionTest() { EXPECT_TRUE(living.empty()); }

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    promise::optional<int> result;

    SuspensionPoint<void> point;
    SuspensionPoint<int> int_point;
    vector<SuspensionPoint<void>> points = {{}};

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
    Promise<void> moving_suspension() {
        function_counts[MOVING_0]++;
        co_await points[0];
        function_counts[MOVING_1]++;
    }
    Promise<void> nested_moving_suspension(int count) {
        if (count <= 0) {
            co_await moving_suspension();
        } else {
            function_counts[NESTED_MOVING_0]++;
            co_await nested_moving_suspension(count - 1);
            function_counts[NESTED_MOVING_1]++;
        }
    }
};

TEST_F(SuspensionTest, simpleSuspension) {
    auto p = simple_suspension();
    EXPECT_FALSE(point);
    p->start();
    EXPECT_TRUE(point);
    expected_counts[SIMPLE_SUSPENSION_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_FALSE(p->done());
    EXPECT_FALSE(p->yielded());
    point.resume();
    EXPECT_FALSE(point);
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

TEST_F(SuspensionTest, recursive_suspension) {
    auto p = nested_moving_suspension(5);
    p->start();
    ASSERT_EQ(living.size(), 7);
    expected_counts[MOVING_0]++;
    expected_counts[NESTED_MOVING_0] += 5;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);

    points[0].resume();
    expected_counts[NESTED_MOVING_1] += 5;
    expected_counts[MOVING_1]++;
    ASSERT_EQ(living.size(), 1);
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(SuspensionTest, movingSuspensionPoint) {
    auto p = moving_suspension();
    p->start();
    ASSERT_EQ(living.size(), 1);
    expected_counts[MOVING_0]++;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);
    auto x = points[0];
    points.clear();

    x.resume();
    expected_counts[MOVING_1]++;
    ASSERT_EQ(living.size(), 1);
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(SuspensionTest, nestedMovingSuspensionPoint) {
    auto p = nested_moving_suspension(5);
    p->start();
    expected_counts[NESTED_MOVING_0] += 5;
    expected_counts[MOVING_0]++;
    EXPECT_FALSE(result);
    EXPECT_EQ(function_counts, expected_counts);
    auto x = points[0];
    points.clear();

    x.resume();
    expected_counts[MOVING_1]++;
    expected_counts[NESTED_MOVING_1] += 5;
    ASSERT_EQ(living.size(), 1);
    EXPECT_EQ(function_counts, expected_counts);
}
