#include <gtest/gtest.h>
#include "promise.h"

using namespace promise;

static auto& living = promise::Coroutine::living;

class PromiseTest : public testing::Test {
public:
    PromiseTest() {
        living.clear();
    }
    ~PromiseTest() {
        EXPECT_TRUE(living.empty());
    }

    Promise<void> empty_co (){
        co_return;
    }
    Promise<int> empty_returning (){
        co_return 1;
    }
    Promise<int> conditional_returning (bool b) {
        if (b) co_return 1;
    }
};

TEST_F(PromiseTest, emptyCoroutine) {
    auto p = empty_co();
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, emptyReturningCoroutine) {
    auto p = empty_returning();
    EXPECT_EQ(living.size(), 1);
    EXPECT_FALSE(p.started());
    EXPECT_FALSE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startEmptyReturningCoroutine) {
    auto p = empty_returning();
    p.start();
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_EQ(p.return_value(), 1);
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startConditionalReturningCoroutine) {
    auto p = conditional_returning(false);
    p.start();
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_FALSE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}

TEST_F(PromiseTest, startEmptyCoroutine) {
    auto p = empty_co();
    p.start();
    EXPECT_EQ(living.size(), 1);
    EXPECT_TRUE(p.started());
    EXPECT_TRUE(p.done());
    EXPECT_TRUE(p.return_value());
    EXPECT_FALSE(p.yield_value());
    EXPECT_FALSE(p.yielded());
}
