#include <coposit/support.hpp>

#include <gtest/gtest.h>

#include <vector>

using namespace coposit;

TEST(SupportTest, PacksMembershipAndSubsetTestsAcrossWords) {
  support larger(130);
  for (const size_t index : {size_t{0}, size_t{63}, size_t{64}, size_t{129}})
    larger.set(index);

  support smaller(130);
  smaller.set(63);
  smaller.set(129);

  EXPECT_TRUE(smaller.is_subset_of(larger));
  EXPECT_FALSE(larger.is_subset_of(smaller));
  EXPECT_EQ(larger.lowest_index(), 0U);
  EXPECT_TRUE(larger.contains(64));
  EXPECT_EQ(larger.cardinality(), 4U);

  larger.reset(64);
  EXPECT_FALSE(larger.contains(64));
  EXPECT_EQ(larger.cardinality(), 3U);

  std::vector<size_t> indices;
  larger.copy_indices_to(indices);
  EXPECT_EQ(indices, (std::vector<size_t>{0, 63, 129}));
}

TEST(SupportTest, CombinesPackedSupportsAcrossWords) {
  support left(130);
  left.set(1);
  left.set(64);

  support right(130);
  right.set(64);
  right.set(129);

  support combined = left;
  combined.add(right);
  EXPECT_TRUE(combined.contains(1));
  EXPECT_TRUE(combined.contains(64));
  EXPECT_TRUE(combined.contains(129));

  combined.intersect_with(right);
  EXPECT_FALSE(combined.contains(1));
  EXPECT_TRUE(combined.contains(64));
  EXPECT_TRUE(combined.contains(129));

  combined.remove(left);
  EXPECT_FALSE(combined.contains(64));
  EXPECT_TRUE(combined.contains(129));

  support replacement(130);
  replacement.set(2);
  combined.swap(replacement);
  EXPECT_TRUE(combined.contains(2));
  EXPECT_TRUE(replacement.contains(129));

  combined.clear();
  EXPECT_TRUE(combined.empty());
}

TEST(SupportTest, RotatesReflectsAndOrdersPackedSupports) {
  support bits(70);
  bits.set(0);
  bits.set(64);
  bits.rotate_one_right();
  EXPECT_TRUE(bits.contains(63));
  EXPECT_TRUE(bits.contains(69));

  bits.reflect();
  EXPECT_TRUE(bits.contains(0));
  EXPECT_TRUE(bits.contains(6));

  support larger(70);
  larger.set(7);
  EXPECT_LT(bits, larger);
  EXPECT_NE(bits, larger);

  bits.set_all();
  for (size_t index = 0; index < bits.dimension(); ++index)
    EXPECT_TRUE(bits.contains(index));
}
