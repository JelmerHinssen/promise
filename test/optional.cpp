#include <gtest/gtest.h>

#include <array>
#include "optional.h"
using namespace promise;

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
    inline static int dtor_count = 0;
    inline static int living_count = 0;
    Unassignable(int& x) : x(x) { living_count++; }
    Unassignable(const Unassignable& other) : x(other.x) { living_count++; }
    ~Unassignable() {
        living_count--;
        dtor_count++;
    }
};
class UnassignableTest : public testing::Test {
   public:
    int& dtor_count = Unassignable::dtor_count;
    int& living_count = Unassignable::living_count;
    UnassignableTest() {
        dtor_count = 0;
        living_count = 0;
    }
    ~UnassignableTest() { EXPECT_EQ(living_count, 0); }
};

TEST_F(UnassignableTest, emptyUnassignable) {
    optional<Unassignable> x{};
    EXPECT_FALSE(x);
    EXPECT_EQ(living_count, 0);
    EXPECT_EQ(dtor_count, 0);
}

TEST_F(UnassignableTest, nonEmptyUnassignable) {
    int y = 2;
    Unassignable u{y};
    EXPECT_EQ(living_count, 1);
    optional<Unassignable> x{u};
    EXPECT_TRUE(x);
    EXPECT_EQ(x->x, y);
    EXPECT_EQ(living_count, 2);
    EXPECT_EQ(dtor_count, 0);
}

TEST_F(UnassignableTest, clearUnassignable) {
    int y = 2;
    Unassignable u{y};
    EXPECT_EQ(living_count, 1);
    optional<Unassignable> x{u};
    EXPECT_TRUE(x);
    EXPECT_EQ(x->x, y);
    EXPECT_EQ(living_count, 2);
    EXPECT_EQ(dtor_count, 0);
    x = {};
    EXPECT_EQ(living_count, 1);
    EXPECT_EQ(dtor_count, 1);
}

TEST_F(UnassignableTest, assignUnassignable) {
    int y = 2;
    Unassignable u{y};
    optional<Unassignable> x{};
    EXPECT_FALSE(x);
    EXPECT_EQ(living_count, 1);
    EXPECT_EQ(dtor_count, 0);
    std::move(x) = u;
    EXPECT_TRUE(x);
    EXPECT_EQ(x->x, y);
    EXPECT_EQ(living_count, 2);
    EXPECT_EQ(dtor_count, 0);
}

TEST_F(UnassignableTest, replaceUnassignable) {
    int y = 2;
    int z = 3;
    Unassignable u{y};
    Unassignable v{z};
    optional<Unassignable> x{};
    EXPECT_FALSE(x);
    EXPECT_EQ(living_count, 2);
    EXPECT_EQ(dtor_count, 0);
    std::move(x) = u;
    EXPECT_TRUE(x);
    EXPECT_EQ(x->x, y);
    EXPECT_EQ(living_count, 3);
    EXPECT_EQ(dtor_count, 0);
    std::move(x) = v;
    EXPECT_TRUE(x);
    EXPECT_EQ(x->x, z);
    EXPECT_EQ(living_count, 3);
    EXPECT_EQ(dtor_count, 1);
}

TEST(Optional, emptyReference) {
    optional<int&> x;
    optional<int&> y;
    EXPECT_FALSE(x);
    EXPECT_EQ(x, x);
    EXPECT_EQ(x, y);
    EXPECT_NE(x, 0);
}

TEST(Optional, compareDifferentTypes) {
    optional<int&> x;
    optional<long long&> y;
    EXPECT_FALSE(x);
    EXPECT_EQ(x, x);
    EXPECT_EQ(x, y);
    EXPECT_NE(x, 0);
}

TEST(Optional, nonEmptyReference) {
    int y = 2;
    optional<int&> x{y};
    EXPECT_TRUE(x);
    EXPECT_EQ(x, 2);
    y = 3;
    EXPECT_EQ(*x, 3);
    *x = 4;
    EXPECT_EQ(y, 4);
}

TEST(Optional, assignReference) {
    int y = 2;
    optional<int&> x{};
    EXPECT_FALSE(x);
    std::move(x) = y;
    EXPECT_TRUE(x);
    EXPECT_EQ(*x, 2);
    y = 3;
    EXPECT_EQ(*x, 3);
    *x = 4;
    EXPECT_EQ(y, 4);
}

TEST(Optional, replaceReference) {
    int y = 2;
    int z = 3;
    optional<int&> x{};
    EXPECT_FALSE(x);
    std::move(x) = y;
    EXPECT_TRUE(x);
    EXPECT_EQ(x, 2);
    y = 4;
    EXPECT_EQ(x, 4);
    x = optional<int&>(z);
    EXPECT_EQ(*x, 3);
    z = 2;
    EXPECT_EQ(*x, 2);
}

TEST(OptionalVoid, empty) {
    optional<void> x{};
    EXPECT_FALSE(x);
}

TEST(OptionalVoid, nonEmpty) {
    optional<void> x{true};
    EXPECT_TRUE(x);
}

TEST(OptionalVoid, set) {
    optional<void> x{};
    x.set();
    EXPECT_TRUE(x);
}

TEST(OptionalVoid, reset) {
    optional<void> x{true};
    x.reset();
    EXPECT_FALSE(x);
}

TEST(OptionalVoid, dereference) {
    auto f = []() {
        optional<void> v{true};
        return *v;
    };
    EXPECT_NO_THROW(f());
}
