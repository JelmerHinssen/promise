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
