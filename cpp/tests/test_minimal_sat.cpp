#include <coposit/minimal_sat.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

TEST(MinimalSat, PropagatesAndBacktracksWithoutLearning) {
  coposit::minimal_sat propagated(3);
  propagated.add_clause({1});
  propagated.add_clause({-1, 2});
  propagated.add_clause({-2, 3});
  ASSERT_TRUE(propagated.solve());
  EXPECT_TRUE(propagated.value(0));
  EXPECT_TRUE(propagated.value(1));
  EXPECT_TRUE(propagated.value(2));

  coposit::minimal_sat unsatisfiable(2);
  unsatisfiable.add_clause({1, 2});
  unsatisfiable.add_clause({-1, 2});
  unsatisfiable.add_clause({1, -2});
  unsatisfiable.add_clause({-1, -2});
  EXPECT_FALSE(unsatisfiable.solve());
  EXPECT_THROW(unsatisfiable.value(0), std::logic_error);
}

TEST(MinimalSat, PropagatesExactCardinalityAndAssumptions) {
  coposit::minimal_sat solver(4);
  solver.add_clause({-1, -2});

  ASSERT_TRUE(solver.solve_exactly(2, {1}));
  EXPECT_TRUE(solver.value(0));
  EXPECT_FALSE(solver.value(1));
  EXPECT_EQ(static_cast<size_t>(solver.value(0)) +
                static_cast<size_t>(solver.value(1)) +
                static_cast<size_t>(solver.value(2)) +
                static_cast<size_t>(solver.value(3)),
            2U);
  EXPECT_FALSE(solver.solve_exactly(2, {1, 2}));
}
