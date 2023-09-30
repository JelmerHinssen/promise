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
        FUNCTION_COUNT
    };

    SuspensionTest() { living.clear(); }
    ~SuspensionTest() { EXPECT_TRUE(living.empty()); }

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

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
