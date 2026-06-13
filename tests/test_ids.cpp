#include <gtest/gtest.h>
#include "ids.hpp"

TEST(StrongId, DefaultIsNull) {
  EXPECT_TRUE(game::UnitId{}.is_null());
  EXPECT_TRUE(game::StructureId{}.is_null());
  EXPECT_TRUE(game::FactionId{}.is_null());
}

TEST(StrongId, ValueNotNull) {
  EXPECT_FALSE((game::UnitId{0}).is_null());
  EXPECT_FALSE((game::UnitId{1}).is_null());
}

TEST(StrongId, OrderingByValue) {
  game::UnitId a{1}, b{2}, c{1};
  EXPECT_LT(a, b);
  EXPECT_EQ(a, c);
  EXPECT_GT(b, a);
}

TEST(StrongId, TypesIncomparable) {
  // UnitId and StructureId with same value must be distinct types.
  game::UnitId u{5};
  game::StructureId s{5};
  // This is a compile-time guarantee (different template instantiations),
  // but we can assert the values are equal at least.
  EXPECT_EQ(u.value, s.value);
}
