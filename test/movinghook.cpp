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
    class empty_hook : public promise::Self<HookOwner>, public promise::ObservablePromise<int, void> {
        promise::Promise<int, void> impl();
        empty_hook(HookOwner* p)
            : promise::Self<HookOwner>(p),
              promise::ObservablePromise<int, void>(promise::bind_member(&empty_hook::impl, this)) {}
        friend class HookOwner;

       public:
        empty_hook(const empty_hook& other)
            : promise::Self<HookOwner>((const Self<HookOwner>&) other),
              promise::ObservablePromise<int, void>(promise::bind_member(&empty_hook::impl, this)) {
            std::cout << "copy 1 called" << std::endl;
            preHooks = other.preHooks;
            postHooks = other.postHooks;
        }
    } empty_hook{this};
    friend class empty_hook;
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
    class empty_hook_hook : public promise::Self<MovingHookTest>, public promise::ObservablePromise<void, void> {
        promise::Promise<void, void> impl();
        empty_hook_hook(MovingHookTest* p)
            : promise::Self<MovingHookTest>(p),
              promise::ObservablePromise<void, void>(promise::bind_member(&empty_hook_hook::impl, this)) {}
        friend class MovingHookTest;

       public:
        empty_hook_hook(const empty_hook_hook& other)
            : promise::Self<MovingHookTest>((const Self<MovingHookTest>&) other),
              promise::ObservablePromise<void, void>(promise::bind_member(&empty_hook_hook::impl, this)) {
            std::cout << "copy 2 called: " << other.self << " -> " << self << std::endl;
            preHooks = other.preHooks;
            postHooks = other.postHooks;
        }
    } empty_hook_hook{this};
    friend class empty_hook_hook;
};

Promise<int> HookOwner::empty_hook::impl() {
    self->that->function_counts[EMPTY_HOOK]++;
    co_return self->id;
}

Promise<void> MovingHookTest::empty_hook_hook::impl() {
    self->function_counts[EMPTY_HOOK_HOOK]++;
    co_return;
}

TEST_F(MovingHookTest, non_moving_empty_hook) {
    EXPECT_EQ(function_counts, expected_counts);
    cout << "non_moving: " << this << endl;
    cout << "owner: " << &owner << endl;
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
