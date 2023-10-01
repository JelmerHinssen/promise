// clang-format off
#include <gtest/gtest.h>
#define GLOBAL_PROMISE
#include "hook.h"
// clang-format on

#include <array>
using namespace std;

#define TEST_HOOK(R, ...) HOOK(R, void, ObservablePromiseTest, __VA_ARGS__)

static auto& living = promise::Coroutine::living;
class ObservablePromiseTest : public testing::Test {
   public:
    enum FunctionNames { EMPTY_HOOK, INT_HOOK, ARG_HOOK, FUNCTION_COUNT };

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    ObservablePromiseTest() { living.clear(); }
    ~ObservablePromiseTest() { EXPECT_TRUE(living.empty()); }

    TEST_HOOK(void, empty_hook);
    TEST_HOOK(int, int_hook);
    TEST_HOOK(int, arg_hook, int, int);
};

Promise<void> ObservablePromiseTest::empty_hook::impl() {
    self->function_counts[EMPTY_HOOK]++;
    co_return;
}

Promise<int> ObservablePromiseTest::int_hook::impl() {
    self->function_counts[INT_HOOK]++;
    co_return 3;
}

Promise<int> ObservablePromiseTest::arg_hook::impl(int a, int b) {
    self->function_counts[ARG_HOOK]++;
    co_return a + b;
}

TEST_F(ObservablePromiseTest, basic) {
    auto p = empty_hook();
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}

TEST_F(ObservablePromiseTest, basicReturn) {
    auto p = int_hook();
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[INT_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 3);
}

TEST_F(ObservablePromiseTest, basicArg) {
    auto p = arg_hook(2, 3);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 5);
}
