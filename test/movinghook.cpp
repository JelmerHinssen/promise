#include <gtest/gtest.h>

#include <array>

#include "hook.h"

using namespace std;
using namespace promise;

#define TEST_OWNER_HOOK(R, ...) HOOK(R, void, HookOwner, __VA_ARGS__)
#define TEST_HOOK(R, ...) HOOK(R, void, MovingHookTest, __VA_ARGS__)

namespace movinghook {

enum FunctionNames { EMPTY_HOOK, EMPTY_HOOK_HOOK, FUNCTION_COUNT };

}  // namespace movinghook
using namespace movinghook;

class MovingHookTest;
class HookOwner {
   public:
    TEST_OWNER_HOOK(int, empty_hook);
    MovingHookTest* that;
    int id = 0;

    HookOwner(MovingHookTest* p) : that(p) {}
};

class MovingHookTest : public testing::Test {
   public:
    array<int, FUNCTION_COUNT> function_counts = {};
    array<int, FUNCTION_COUNT> expected_counts = {};

    SuspensionPoint<void> point;
    int hook_value = -1;
    array<int, 4> post_hook_args{};
    HookOwner owner{this};
    TEST_HOOK(void, empty_hook_hook);
    friend class empty_hook_hook;
};

class MovingHookOwner : public MovingSelf<MovingHookOwner> {
    public:
    MovingHookOwner(): MovingSelf<MovingHookOwner>(this) {}
    void add_hook(HookOwner& owner) {
        owner.empty_hook.postHooks += movableHook<Promise<void>>([](MovingHookOwner* that) -> Promise<void> {
            that->result = that;
            co_return;
        });
    }
    MovingHookOwner* result = nullptr;
};

Promise<int>
HookOwner::empty_hook::impl() {
    self->that->function_counts[EMPTY_HOOK]++;
    co_return self->id;
}

Promise<void> MovingHookTest::empty_hook_hook::impl() {
    self->function_counts[EMPTY_HOOK_HOOK]++;
    co_return;
}

TEST_F(MovingHookTest, non_moving_empty_hook) {
    EXPECT_EQ(function_counts, expected_counts);
    owner.empty_hook.preHooks += empty_hook_hook;
    auto p = owner.empty_hook();
    p->start();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(p->returned_value(), 0);
}

TEST_F(MovingHookTest, moving_empty_hook) {
    EXPECT_EQ(function_counts, expected_counts);
    owner.empty_hook.preHooks += empty_hook_hook;
    HookOwner other = owner;
    other.id = 1;
    auto p = other.empty_hook();
    p->start();
    expected_counts[EMPTY_HOOK_HOOK]++;
    expected_counts[EMPTY_HOOK]++;
    EXPECT_EQ(function_counts, expected_counts);
    EXPECT_EQ(p->returned_value(), 1);
}

TEST_F(MovingHookTest, moving_hook_basic) {
    MovingHookOwner thing;
    thing.add_hook(owner);
    EXPECT_EQ(thing.result, nullptr);
    auto p = owner.empty_hook();
    p->start();
    EXPECT_EQ(thing.result, &thing);
}

TEST_F(MovingHookTest, moving_hook_moving) {
    MovingHookOwner thing;
    thing.add_hook(owner);
    MovingHookOwner other = std::move(thing);
    EXPECT_EQ(thing.result, nullptr);
    auto p = owner.empty_hook();
    p->start();
    EXPECT_EQ(thing.result, nullptr);
    EXPECT_EQ(other.result, &other);
}
