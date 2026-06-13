#include <gtest/gtest.h>
#include "rng.hpp"

TEST(Rng, SameSeedReproducible) {
  game::Rng a{42}, b{42};
  for (int i = 0; i < 10'000; ++i)
    EXPECT_EQ(a.next(), b.next());
}

TEST(Rng, DifferentSeedsDiffer) {
  game::Rng a{1}, b{2};
  EXPECT_NE(a.next(), b.next());
}

TEST(Rng, DrawCountTracked) {
  game::Rng r{0};
  EXPECT_EQ(r.draw_count(), 0u);
  r.next();
  EXPECT_EQ(r.draw_count(), 1u);
  r.next_u32(10);
  EXPECT_EQ(r.draw_count(), 2u);
}

TEST(Rng, NextU32InBounds) {
  game::Rng r{99999};
  for (int i = 0; i < 50'000; ++i)
    EXPECT_LT(r.next_u32(100), 100u);
}

TEST(Rng, NextU32ZeroAndOne) {
  game::Rng r{0};
  EXPECT_EQ(r.next_u32(0), 0u);
  EXPECT_EQ(r.next_u32(1), 0u);
}
