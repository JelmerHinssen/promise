#include <gtest/gtest.h>
#include <array>
#include "promise.h"

using namespace promise;

TEST(Optional, emptyVoid) {
    optional<void> x{};
    EXPECT_FALSE(x);
}

TEST(Optional, nonEmptyVoid) {
    optional<void> x = true;
    EXPECT_TRUE(x);
}

TEST(Optional, emptyInt) {
    optional<int> x{};
    EXPECT_FALSE(x);
}

TEST(Optional, nonEmptyInt) {
    optional<int> x = 0;
    EXPECT_TRUE(x);
    EXPECT_EQ(x, 0);
}

struct Unassignable {
    int& x;
};

TEST(Optional, emptyUnassignable) {
    optional<Unassignable> x{};
    EXPECT_FALSE(x);
}

TEST(Optional, nonEmptyUnassignable) {
    int y = 2;
    Unassignable u{y};
    optional<Unassignable> x = u;
    EXPECT_TRUE(x);
    EXPECT_EQ(x->x, y);
}
