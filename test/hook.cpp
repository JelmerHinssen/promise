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
    enum FunctionNames {
        EMPTY_HOOK,
        INT_HOOK,
        ARG_HOOK,
        EMPTY_HOOK_HOOK,
        WAITING_HOOK_HOOK_0,
        WAITING_HOOK_HOOK_1,
        ARG_HOOK_HOOK,
        ARG_POST_HOOK,
        RESULT_POST_HOOK,
        FUNCTION_COUNT
    };

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    ObservablePromiseTest() { living.clear(); }
    ~ObservablePromiseTest() { EXPECT_TRUE(living.empty()); }

    SuspensionPoint<void> point;
    int hook_value = -1;
    array<int, 4> post_hook_args{};

    TEST_HOOK(void, empty_hook);
    TEST_HOOK(void, empty_hook_hook);
    TEST_HOOK(void, waiting_hook_hook);
    TEST_HOOK(int, int_hook);
    TEST_HOOK(int, arg_hook, int, int);
    TEST_HOOK(void, arg_hook_hook, int, int);
    TEST_HOOK(void, arg_post_hook, int, int, int);
    TEST_HOOK(void, result_post_hook, int);
    TEST_HOOK(int, ambiguous_hook, int);
};

Promise<void> ObservablePromiseTest::empty_hook::impl() {
    self->function_counts[EMPTY_HOOK]++;
    co_return;
}

Promise<void> ObservablePromiseTest::empty_hook_hook::impl() {
    self->function_counts[EMPTY_HOOK_HOOK]++;
    co_return;
}

Promise<void> ObservablePromiseTest::waiting_hook_hook::impl() {
    self->function_counts[WAITING_HOOK_HOOK_0]++;
    co_await self->point;
    self->function_counts[WAITING_HOOK_HOOK_1]++;
}

Promise<int> ObservablePromiseTest::int_hook::impl() {
    self->function_counts[INT_HOOK]++;
    co_return 3;
}

Promise<int> ObservablePromiseTest::arg_hook::impl(int a, int b) {
    self->function_counts[ARG_HOOK]++;
    co_return a + b;
}

Promise<void> ObservablePromiseTest::arg_hook_hook::impl(int a, int b) {
    self->function_counts[ARG_HOOK_HOOK]++;
    self->hook_value = a + b;
    co_return;
}

Promise<void> ObservablePromiseTest::arg_post_hook::impl(int r, int a, int b) {
    self->function_counts[ARG_POST_HOOK]++;
    self->post_hook_args[0] = r;
    self->post_hook_args[1] = a;
    self->post_hook_args[2] = b;
    co_return;
}

Promise<void> ObservablePromiseTest::result_post_hook::impl(int r) {
    self->function_counts[RESULT_POST_HOOK]++;
    self->post_hook_args[3] = r;
    co_return;
}

Promise<int> ObservablePromiseTest::ambiguous_hook::impl(int r) {
    co_return 2 * r;
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

TEST_F(ObservablePromiseTest, basicHooked) {
    auto p = empty_hook();
    empty_hook.preHooks += empty_hook_hook;
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}

TEST_F(ObservablePromiseTest, basicPostHooked) {
    auto p = empty_hook();
    empty_hook.postHooks += empty_hook_hook;
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[EMPTY_HOOK]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}

TEST_F(ObservablePromiseTest, waitingHooked) {
    auto p = empty_hook();
    empty_hook.preHooks += empty_hook_hook;
    empty_hook.preHooks += waiting_hook_hook;
    empty_hook.preHooks += empty_hook_hook;
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[WAITING_HOOK_HOOK_0]++;
    EXPECT_EQ(function_counts, expected_counts);

    point.resume();
    expected_counts[WAITING_HOOK_HOOK_1]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}

TEST_F(ObservablePromiseTest, waitingHookedPrePost) {
    auto p = empty_hook();
    empty_hook.preHooks += empty_hook_hook;
    empty_hook.preHooks += waiting_hook_hook;
    empty_hook.preHooks += empty_hook_hook;
    empty_hook.postHooks += empty_hook_hook;
    empty_hook.postHooks += waiting_hook_hook;
    empty_hook.postHooks += empty_hook_hook;
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[WAITING_HOOK_HOOK_0]++;
    EXPECT_EQ(function_counts, expected_counts);

    point.resume();
    expected_counts[WAITING_HOOK_HOOK_1]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[WAITING_HOOK_HOOK_0]++;

    point.resume();
    expected_counts[WAITING_HOOK_HOOK_1]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    
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

TEST_F(ObservablePromiseTest, hookedArg) {
    arg_hook.preHooks += arg_hook_hook;
    auto p = arg_hook(2, 3);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    EXPECT_EQ(hook_value, -1);
    p->start();
    expected_counts[ARG_HOOK_HOOK]++;
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(hook_value, 5);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 5);
}

TEST_F(ObservablePromiseTest, postHookedArg) {
    arg_hook.postHooks += arg_hook_hook;
    auto p = arg_hook(2, 3);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    EXPECT_EQ(hook_value, -1);
    p->start();
    expected_counts[ARG_HOOK_HOOK]++;
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(hook_value, 5);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 5);
}

TEST_F(ObservablePromiseTest, resultHookedArg) {
    arg_hook.postHooks += arg_post_hook;
    arg_hook.postHooks += result_post_hook;
    auto p = arg_hook(3, 8);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    p->start();
    expected_counts[ARG_HOOK]++;
    expected_counts[ARG_POST_HOOK]++;
    expected_counts[RESULT_POST_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    std::array<int, 4> expected_values{11, 3, 8, 11};
    EXPECT_EQ(post_hook_args, expected_values);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 11);
}

TEST_F(ObservablePromiseTest, ambiguousHook) {
    ambiguous_hook.postHooks.argHook(result_post_hook);
    auto p = ambiguous_hook(6);
    p->start();
    std::array<int, 4> expected_values{0, 0, 0, 6};
    EXPECT_EQ(post_hook_args, expected_values);
    EXPECT_EQ(p->returned_value(), 12);
}

TEST_F(ObservablePromiseTest, noArghookedArg) {
    arg_hook.preHooks += arg_hook_hook;
    arg_hook.preHooks += empty_hook_hook;
    auto p = arg_hook(2, 3);
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p->started());
    EXPECT_EQ(hook_value, -1);
    p->start();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[ARG_HOOK_HOOK]++;
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(hook_value, 5);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 5);
}
