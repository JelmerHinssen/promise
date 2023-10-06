// clang-format off
#include <gtest/gtest.h>
#define GLOBAL_PROMISE
#include "functionhook.h"
// clang-format on

#include <array>
using namespace std;

#define TEST_HOOK(R, ...) NON_BLOCKING_HOOK(R, ObservableFunctionTest, __VA_ARGS__)

class ObservableFunctionTest : public testing::Test {
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

void ObservableFunctionTest::empty_hook::impl() {
    self->function_counts[EMPTY_HOOK]++;
}

void ObservableFunctionTest::empty_hook_hook::impl() {
    self->function_counts[EMPTY_HOOK_HOOK]++;
}

int ObservableFunctionTest::int_hook::impl() {
    self->function_counts[INT_HOOK]++;
    return 3;
}

void ObservableFunctionTest::waiting_hook_hook::impl() {
    self->function_counts[WAITING_HOOK_HOOK_0]++;
    self->function_counts[WAITING_HOOK_HOOK_1]++;
}

int ObservableFunctionTest::arg_hook::impl(int a, int b) {
    self->function_counts[ARG_HOOK]++;
    return a + b;
}

void ObservableFunctionTest::arg_hook_hook::impl(int a, int b) {
    self->function_counts[ARG_HOOK_HOOK]++;
    self->hook_value = a + b;
}

void ObservableFunctionTest::arg_post_hook::impl(int r, int a, int b) {
    self->function_counts[ARG_POST_HOOK]++;
    self->post_hook_args[0] = r;
    self->post_hook_args[1] = a;
    self->post_hook_args[2] = b;
}

void ObservableFunctionTest::result_post_hook::impl(int r) {
    self->function_counts[RESULT_POST_HOOK]++;
    self->post_hook_args[3] = r;
}

int ObservableFunctionTest::ambiguous_hook::impl(int r) {
    return 2 * r;
}

TEST_F(ObservableFunctionTest, basic) {
    empty_hook();
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(ObservableFunctionTest, basicHooked) {
    empty_hook.preHooks += empty_hook_hook;
    empty_hook();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(ObservableFunctionTest, basicPostHooked) {
    empty_hook.postHooks += empty_hook_hook;
    empty_hook();
    expected_counts[EMPTY_HOOK]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
}

TEST_F(ObservableFunctionTest, waitingHooked) {
    empty_hook.preHooks += empty_hook_hook;
    empty_hook.preHooks += waiting_hook_hook;
    empty_hook.preHooks += empty_hook_hook;
    empty_hook();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[WAITING_HOOK_HOOK_0]++;
    expected_counts[WAITING_HOOK_HOOK_1]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(ObservableFunctionTest, waitingHookedPrePost) {
    empty_hook.preHooks += empty_hook_hook;
    empty_hook.preHooks += waiting_hook_hook;
    empty_hook.preHooks += empty_hook_hook;
    empty_hook.postHooks += empty_hook_hook;
    empty_hook.postHooks += waiting_hook_hook;
    empty_hook.postHooks += empty_hook_hook;
    empty_hook();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[WAITING_HOOK_HOOK_0]++;
    expected_counts[WAITING_HOOK_HOOK_1]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[WAITING_HOOK_HOOK_0]++;
    expected_counts[WAITING_HOOK_HOOK_1]++;
    expected_counts[EMPTY_HOOK_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
}

TEST_F(ObservableFunctionTest, basicReturn) {
    auto v = int_hook();
    expected_counts[INT_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(v, 3);
}

TEST_F(ObservableFunctionTest, basicArg) {
    auto p = arg_hook(2, 3);
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(p, 5);
}

TEST_F(ObservableFunctionTest, hookedArg) {
    arg_hook.preHooks += arg_hook_hook;
    auto p = arg_hook(2, 3);
    expected_counts[ARG_HOOK_HOOK]++;
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(hook_value, 5);
    EXPECT_EQ(p, 5);
}

TEST_F(ObservableFunctionTest, postHookedArg) {
    arg_hook.postHooks += arg_hook_hook;
    auto p = arg_hook(2, 3);
    expected_counts[ARG_HOOK_HOOK]++;
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(hook_value, 5);
    EXPECT_EQ(p, 5);
}

TEST_F(ObservableFunctionTest, resultHookedArg) {
    arg_hook.postHooks += arg_post_hook;
    arg_hook.postHooks += result_post_hook;
    auto p = arg_hook(3, 8);
    expected_counts[ARG_HOOK]++;
    expected_counts[ARG_POST_HOOK]++;
    expected_counts[RESULT_POST_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    std::array<int, 4> expected_values{11, 3, 8, 11};
    EXPECT_EQ(post_hook_args, expected_values);
    EXPECT_EQ(p, 11);
}

TEST_F(ObservableFunctionTest, ambiguousHook) {
    ambiguous_hook.postHooks.argHook(result_post_hook);
    auto p = ambiguous_hook(6);
    std::array<int, 4> expected_values{0, 0, 0, 6};
    EXPECT_EQ(post_hook_args, expected_values);
    EXPECT_EQ(p, 12);
}

TEST_F(ObservableFunctionTest, noArghookedArg) {
    arg_hook.preHooks += arg_hook_hook;
    arg_hook.preHooks += empty_hook_hook;
    auto p = arg_hook(2, 3);
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[ARG_HOOK_HOOK]++;
    expected_counts[ARG_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(hook_value, 5);
    EXPECT_EQ(p, 5);
}
