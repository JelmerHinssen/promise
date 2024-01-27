// clang-format off
#include <gtest/gtest.h>
#define GLOBAL_PROMISE
#include "hook.h"
// clang-format on

#include <array>
using namespace std;

#define TEST_HOOK(R, ...) HOOK(R, void, IDHookTest, __VA_ARGS__)

static auto& living = promise::Coroutine::living;
class IDHookTest : public testing::Test {
    public:
     enum FunctionNames {
        EMPTY_HOOK,
        INT_HOOK,
        FUNCTION_COUNT
    };

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    IDHookTest() { living.clear(); }
    ~IDHookTest() { EXPECT_TRUE(living.empty()); }

    SuspensionPoint<void> point;
    int hook_value = -1;
    array<int, 4> post_hook_args{};

    TEST_HOOK(void, empty_hook);
    TEST_HOOK(int, int_hook);
};

Promise<void> IDHookTest::empty_hook::impl() {
    self->function_counts[EMPTY_HOOK]++;
    co_return;
}

Promise<int> IDHookTest::int_hook::impl() {
    self->function_counts[INT_HOOK]++;
    co_return 3;
}

TEST_F(IDHookTest, remove_pre_hook) {
    auto p = empty_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(empty_hook.preHooks += addCalled(i));
    }
    EXPECT_TRUE(empty_hook.preHooks.remove(ids[2]));
    EXPECT_TRUE(empty_hook.preHooks.remove(ids[4]));
    EXPECT_FALSE(empty_hook.preHooks.remove(ids[4]));
    vector<int> expected = {0, 1, 3, 5};
    p->start();
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(called, expected);
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}

TEST_F(IDHookTest, set_post_hook) {
    auto p = empty_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(empty_hook.postHooks += addCalled(i));
    }
    EXPECT_TRUE(empty_hook.postHooks.set(ids[2], addCalled(10)));
    EXPECT_TRUE(empty_hook.postHooks.remove(ids[4]));
    EXPECT_FALSE(empty_hook.postHooks.set(ids[4], addCalled(10)));
    vector<int> expected = {0, 1, 10, 3, 5};
    p->start();
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(called, expected);
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}

TEST_F(IDHookTest, remove_returning_hook) {
    auto p = int_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(int_hook.postHooks += addCalled(i));
    }
    EXPECT_TRUE(int_hook.postHooks.remove(ids[2]));
    EXPECT_TRUE(int_hook.postHooks.remove(ids[4]));
    EXPECT_FALSE(int_hook.postHooks.remove(ids[4]));
    vector<int> expected = {0, 1, 3, 5};
    p->start();
    expected_counts[INT_HOOK]++;
    EXPECT_EQ(called, expected);
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 3);
}

TEST_F(IDHookTest, set_returning_hook) {
    auto p = int_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(int_hook.preHooks += addCalled(i));
    }
    EXPECT_TRUE(int_hook.preHooks.set(ids[2], addCalled(10)));
    EXPECT_TRUE(int_hook.preHooks.remove(ids[4]));
    EXPECT_FALSE(int_hook.preHooks.set(ids[4], addCalled(10)));
    vector<int> expected = {0, 1, 10, 3, 5};
    p->start();
    expected_counts[INT_HOOK]++;
    EXPECT_EQ(called, expected);
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 3);
}
