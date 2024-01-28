// clang-format off
#include <gtest/gtest.h>
#define GLOBAL_PROMISE
#include "hook.h"
// clang-format on

#include <array>
using namespace std;

#define TEST_HOOK(R, ...) HOOK(R, void, IDHookTest, __VA_ARGS__)
#define TEST_OWNER_HOOK(R, ...) HOOK(R, void, IDHookOwner, __VA_ARGS__)

static auto& living = promise::Coroutine::living;

namespace idhook {

enum FunctionNames { EMPTY_HOOK, INT_HOOK, FUNCTION_COUNT };

}  // namespace idhook

using namespace idhook;
class IDHookTest;
class IDHookOwner {
   public:
    TEST_OWNER_HOOK(void, empty_hook);
    TEST_OWNER_HOOK(int, int_hook);
    IDHookTest* that;

    IDHookOwner(IDHookTest* p) : that(p) {}
};

class IDHookTest : public testing::Test {
   public:
    enum FunctionNames { EMPTY_HOOK, INT_HOOK, FUNCTION_COUNT };

    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    IDHookTest() { living.clear(); }
    ~IDHookTest() { EXPECT_TRUE(living.empty()); }

    SuspensionPoint<void> point;
    int hook_value = -1;
    array<int, 4> post_hook_args{};

    IDHookOwner owner{this};
};

Promise<void> IDHookOwner::empty_hook::impl() {
    self->that->function_counts[EMPTY_HOOK]++;
    co_return;
}

Promise<int> IDHookOwner::int_hook::impl() {
    self->that->function_counts[INT_HOOK]++;
    co_return 3;
}

TEST_F(IDHookTest, remove_pre_hook) {
    auto p = owner.empty_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(owner.empty_hook.preHooks += addCalled(i));
    }
    EXPECT_TRUE(owner.empty_hook.preHooks.remove(ids[2]));
    EXPECT_TRUE(owner.empty_hook.preHooks.remove(ids[4]));
    EXPECT_FALSE(owner.empty_hook.preHooks.remove(ids[4]));
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
    auto p = owner.empty_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(owner.empty_hook.postHooks += addCalled(i));
    }
    EXPECT_TRUE(owner.empty_hook.postHooks.set(ids[2], addCalled(10)));
    EXPECT_TRUE(owner.empty_hook.postHooks.remove(ids[4]));
    EXPECT_FALSE(owner.empty_hook.postHooks.set(ids[4], addCalled(10)));
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
    auto p = owner.int_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(owner.int_hook.postHooks += addCalled(i));
    }
    EXPECT_TRUE(owner.int_hook.postHooks.remove(ids[2]));
    EXPECT_TRUE(owner.int_hook.postHooks.remove(ids[4]));
    EXPECT_FALSE(owner.int_hook.postHooks.remove(ids[4]));
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
    auto p = owner.int_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(owner.int_hook.preHooks += addCalled(i));
    }
    EXPECT_TRUE(owner.int_hook.preHooks.set(ids[2], addCalled(10)));
    EXPECT_TRUE(owner.int_hook.preHooks.remove(ids[4]));
    EXPECT_FALSE(owner.int_hook.preHooks.set(ids[4], addCalled(10)));
    vector<int> expected = {0, 1, 10, 3, 5};
    p->start();
    expected_counts[INT_HOOK]++;
    EXPECT_EQ(called, expected);
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_EQ(p->returned_value(), 3);
}

TEST_F(IDHookTest, set_pre_hook_after_copy) {
    auto p = owner.empty_hook();
    vector<int> called;
    auto addCalled = [&called](int value) {
        return [&called, value]() -> Promise<void> {
            called.push_back(value);
            co_return;
        };
    };
    vector<size_t> ids;
    for (int i = 0; i < 6; i++) {
        ids.push_back(owner.empty_hook.preHooks += addCalled(i));
    }
    EXPECT_TRUE(owner.empty_hook.preHooks.set(ids[2], addCalled(10)));
    EXPECT_TRUE(owner.empty_hook.preHooks.remove(ids[4]));
    EXPECT_FALSE(owner.empty_hook.preHooks.set(ids[4], addCalled(10)));
    auto copy = owner;
    copy.empty_hook.preHooks.set(ids[0], addCalled(9));
    auto q = copy.empty_hook();
    p->start();
    q->start();
    vector<int> expected = {0, 1, 10, 3, 5, 9, 1, 10, 3, 5};
    expected_counts[EMPTY_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(called, expected);
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_TRUE(p->started());
    EXPECT_TRUE(p->done());
    EXPECT_TRUE(p->returned_value());
}
