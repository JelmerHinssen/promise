// clang-format off
#include <gtest/gtest.h>
#include "promise.h"
// clang-format on

#include <array>

using namespace promise;
using namespace std;

static auto& living = promise::Coroutine::living;

class RangeSuspensionTest : public testing::Test {
   public:
    enum FunctionNames {
        RANGE_SUSPENSION_0,
        RANGE_SUSPENSION_1,
        ARRAY_RANGE_SUSPENSION_0,
        ARRAY_RANGE_SUSPENSION_1,
        FUNCTION_COUNT
    };

    RangeSuspensionTest() { living.clear(); }
    ~RangeSuspensionTest() { EXPECT_TRUE(living.empty()); }

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    promise::optional<int> result;

    SuspensionPoint<void> point;
    vector<SuspensionPoint<void>> points{3};
    array<SuspensionPoint<void>, 3> points_array{};

    Promise<void> range_suspension() {
        function_counts[RANGE_SUSPENSION_0]++;
        co_await points;
        function_counts[RANGE_SUSPENSION_1]++;
    }
    Promise<void> array_range_suspension() {
        function_counts[ARRAY_RANGE_SUSPENSION_0]++;
        co_await points_array;
        function_counts[ARRAY_RANGE_SUSPENSION_1]++;
    }
};

TEST_F(RangeSuspensionTest, range_wait) {
    auto p = range_suspension();
    p->start();
    expected_counts[RANGE_SUSPENSION_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    points[0].resume();
    EXPECT_EQ(function_counts, expected_counts);
    points[1].resume();
    EXPECT_EQ(function_counts, expected_counts);
    points[2].resume();
    expected_counts[RANGE_SUSPENSION_1]++;
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(RangeSuspensionTest, array_range_wait) {
    auto p = array_range_suspension();
    p->start();
    expected_counts[ARRAY_RANGE_SUSPENSION_0]++;
    EXPECT_EQ(function_counts, expected_counts);
    points_array[0].resume();
    EXPECT_EQ(function_counts, expected_counts);
    points_array[1].resume();
    EXPECT_EQ(function_counts, expected_counts);
    points_array[2].resume();
    expected_counts[ARRAY_RANGE_SUSPENSION_1]++;
    EXPECT_EQ(function_counts, expected_counts);
}
