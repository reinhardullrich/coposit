#include <coposit/fractional_milp.hpp>

#include <gtest/gtest.h>

#include <flint/fmpz.h>

#include <chrono>
#include <limits>
#include <random>
#include <vector>

namespace {

using coposit::fractional_milp::constraint_sense;
using coposit::fractional_milp::problem;
using coposit::fractional_milp::solve_options;
using coposit::fractional_milp::solve_status;
using coposit::fractional_milp::solver;
using coposit::fractional_milp::variable_type;

std::vector<coposit::integer> coefficients(size_t count, size_t variable,
                                           slong value) {
  std::vector<coposit::integer> result(count);
  result[variable] = coposit::integer(value);
  return result;
}

TEST(FractionalMilpTest, ReturnsAnExactFractionalContinuousOptimum) {
  problem input(1);
  input.set_objective_coefficient(0, coposit::integer(1));
  input.add_constraint(coefficients(1, 0, 3), constraint_sense::less_equal,
                       coposit::integer(2));

  const auto result = solver().solve(input);

  ASSERT_EQ(result.status, solve_status::optimal);
  ASSERT_EQ(result.point.size(), 1U);
  EXPECT_EQ(result.point[0].to_string(), "2/3");
  EXPECT_EQ(result.objective.to_string(), "2/3");
  EXPECT_GT(result.exact_lp_solves, 0U);
}

TEST(FractionalMilpTest,
     FloatingRoundingCannotCreateAnIntegerIncumbentOrPrune) {
  coposit::integer large(1);
  fmpz_mul_2exp(large.native_handle(), large.native_handle(), 200);
  coposit::integer one_less(large);
  fmpz_sub_ui(one_less.native_handle(), one_less.native_handle(), 1);

  problem input(1);
  input.set_variable_type(0, variable_type::binary);
  input.set_objective_coefficient(0, coposit::integer(1));
  std::vector<coposit::integer> row(1);
  row[0] = large;
  input.add_constraint(std::move(row), constraint_sense::less_equal,
                       std::move(one_less));

  const auto result = solver().solve(input);

  ASSERT_EQ(result.status, solve_status::optimal);
  EXPECT_EQ(result.objective.to_string(), "0");
  ASSERT_EQ(result.point.size(), 1U);
  EXPECT_EQ(result.point[0].to_string(), "0");
  EXPECT_GE(result.nodes, 3U);
  EXPECT_GT(result.exact_lp_solves, 0U);
}

TEST(FractionalMilpTest, SolvesMixedBinaryContinuousProblems) {
  problem input(2);
  input.set_variable_type(1, variable_type::binary);
  input.set_objective_coefficient(0, coposit::integer(1));
  std::vector<coposit::integer> row(2);
  row[0] = coposit::integer(1);
  row[1] = coposit::integer(-2);
  input.add_constraint(std::move(row), constraint_sense::less_equal,
                       coposit::integer(0));

  const auto result = solver().solve(input);

  ASSERT_EQ(result.status, solve_status::optimal);
  ASSERT_EQ(result.point.size(), 2U);
  EXPECT_EQ(result.point[0].to_string(), "2");
  EXPECT_EQ(result.point[1].to_string(), "1");
  EXPECT_EQ(result.objective.to_string(), "2");
}

TEST(FractionalMilpTest, ProvesInfeasibilityExactly) {
  problem input(1);
  input.set_variable_type(0, variable_type::binary);
  input.add_constraint(coefficients(1, 0, 1), constraint_sense::greater_equal,
                       coposit::integer(1));
  input.add_constraint(coefficients(1, 0, 1), constraint_sense::less_equal,
                       coposit::integer(0));

  const auto result = solver().solve(input);

  EXPECT_EQ(result.status, solve_status::infeasible);
  EXPECT_TRUE(result.point.empty());
  EXPECT_GT(result.exact_lp_solves, 0U);
}

TEST(FractionalMilpTest, ProvesUnboundednessExactly) {
  problem input(1);
  input.set_objective_coefficient(0, coposit::integer(1));

  const auto result = solver().solve(input);

  EXPECT_EQ(result.status, solve_status::unbounded);
  EXPECT_TRUE(result.point.empty());
  EXPECT_GT(result.exact_lp_solves, 0U);
}

TEST(FractionalMilpTest, ResourceLimitIsNeverReportedAsAProof) {
  problem input(1);
  input.set_variable_type(0, variable_type::binary);
  input.set_objective_coefficient(0, coposit::integer(1));

  solve_options options;
  options.node_limit = 0;
  const auto result = solver().solve(input, options);

  EXPECT_EQ(result.status, solve_status::interrupted);
  EXPECT_TRUE(result.point.empty());
}

TEST(FractionalMilpTest, RejectsInvalidPublicModels) {
  EXPECT_THROW(problem(0), std::invalid_argument);
  problem input(2);
  EXPECT_THROW(input.add_constraint(std::vector<coposit::integer>(1),
                                    constraint_sense::equal,
                                    coposit::integer(0)),
               std::invalid_argument);
  EXPECT_THROW(input.set_upper_bound(2, coposit::integer(1)),
               std::out_of_range);
  EXPECT_THROW(input.set_upper_bound(0, coposit::integer(-1)),
               std::invalid_argument);
}

TEST(FractionalMilpTest, MatchesBruteForceAcrossSmallIntegerBinaryProblems) {
  std::mt19937 generator(1729);
  std::uniform_int_distribution<int> coefficient(-3, 3);
  std::uniform_int_distribution<int> right_hand_side(-4, 4);
  std::uniform_int_distribution<int> sense_choice(0, 2);

  for (size_t trial = 0; trial < 300; ++trial) {
    constexpr size_t dimension = 5;
    constexpr size_t constraint_count = 5;
    problem input(dimension);
    std::vector<long> objective(dimension);
    for (size_t variable = 0; variable < dimension; ++variable) {
      input.set_variable_type(variable, variable_type::binary);
      objective[variable] = coefficient(generator);
      input.set_objective_coefficient(variable,
                                      coposit::integer(objective[variable]));
    }

    struct brute_constraint {
      std::vector<long> coefficients;
      constraint_sense sense;
      long right_hand_side;
    };
    std::vector<brute_constraint> constraints;
    for (size_t row = 0; row < constraint_count; ++row) {
      std::vector<coposit::integer> exact_coefficients(dimension);
      std::vector<long> brute_coefficients(dimension);
      for (size_t variable = 0; variable < dimension; ++variable) {
        brute_coefficients[variable] = coefficient(generator);
        exact_coefficients[variable] =
            coposit::integer(brute_coefficients[variable]);
      }
      const auto sense = static_cast<constraint_sense>(sense_choice(generator));
      const long rhs = right_hand_side(generator);
      input.add_constraint(std::move(exact_coefficients), sense,
                           coposit::integer(rhs));
      constraints.push_back({std::move(brute_coefficients), sense, rhs});
    }

    bool feasible = false;
    long optimum = std::numeric_limits<long>::min();
    for (size_t mask = 0; mask < (size_t{1} << dimension); ++mask) {
      bool accepted = true;
      for (const auto &constraint : constraints) {
        long value = 0;
        for (size_t variable = 0; variable < dimension; ++variable)
          if ((mask >> variable) & 1U)
            value += constraint.coefficients[variable];
        accepted &= constraint.sense == constraint_sense::less_equal
                        ? value <= constraint.right_hand_side
                    : constraint.sense == constraint_sense::greater_equal
                        ? value >= constraint.right_hand_side
                        : value == constraint.right_hand_side;
      }
      if (!accepted)
        continue;
      feasible = true;
      long value = 0;
      for (size_t variable = 0; variable < dimension; ++variable)
        if ((mask >> variable) & 1U)
          value += objective[variable];
      optimum = std::max(optimum, value);
    }

    const auto result = solver().solve(input);
    if (!feasible) {
      ASSERT_EQ(result.status, solve_status::infeasible) << "trial " << trial;
    } else {
      ASSERT_EQ(result.status, solve_status::optimal) << "trial " << trial;
      EXPECT_EQ(result.objective.to_string(), std::to_string(optimum))
          << "trial " << trial;
    }
  }
}

problem two_by_two_anstreicher(long off_diagonal) {
  // Variables are x1, x2, y1, y2, gamma. For this matrix m1=m2=1 and theta=2.
  problem input(5);
  input.set_variable_type(2, variable_type::binary);
  input.set_variable_type(3, variable_type::binary);
  input.set_objective_coefficient(4, coposit::integer(1));

  std::vector<coposit::integer> first_row(5);
  first_row[0] = coposit::integer(1);
  first_row[1] = coposit::integer(off_diagonal);
  first_row[2] = coposit::integer(1);
  first_row[4] = coposit::integer(1);
  input.add_constraint(std::move(first_row), constraint_sense::less_equal,
                       coposit::integer(1));

  std::vector<coposit::integer> second_row(5);
  second_row[0] = coposit::integer(off_diagonal);
  second_row[1] = coposit::integer(1);
  second_row[3] = coposit::integer(1);
  second_row[4] = coposit::integer(1);
  input.add_constraint(std::move(second_row), constraint_sense::less_equal,
                       coposit::integer(1));

  std::vector<coposit::integer> first_link(5);
  first_link[0] = coposit::integer(1);
  first_link[2] = coposit::integer(-1);
  input.add_constraint(std::move(first_link), constraint_sense::less_equal,
                       coposit::integer(0));

  std::vector<coposit::integer> second_link(5);
  second_link[1] = coposit::integer(1);
  second_link[3] = coposit::integer(-1);
  input.add_constraint(std::move(second_link), constraint_sense::less_equal,
                       coposit::integer(0));

  std::vector<coposit::integer> support_size(5);
  support_size[2] = coposit::integer(1);
  support_size[3] = coposit::integer(1);
  input.add_constraint(std::move(support_size), constraint_sense::greater_equal,
                       coposit::integer(2));
  return input;
}

TEST(FractionalMilpTest, SolvesTheFullAnstreicherMilpShapeExactly) {
  const auto copositive = solver().solve(two_by_two_anstreicher(0));
  ASSERT_EQ(copositive.status, solve_status::optimal);
  EXPECT_EQ(copositive.objective.to_string(), "0");

  const auto noncopositive = solver().solve(two_by_two_anstreicher(-2));
  ASSERT_EQ(noncopositive.status, solve_status::optimal);
  EXPECT_EQ(noncopositive.objective.to_string(), "1");
}

TEST(FractionalMilpTest, StopsOnlyAfterAnExactPositiveIncumbent) {
  solve_options options;
  options.stop_on_positive_objective = true;

  const auto copositive = solver().solve(two_by_two_anstreicher(0), options);
  ASSERT_EQ(copositive.status, solve_status::optimal);
  EXPECT_EQ(copositive.objective.to_string(), "0");

  const auto noncopositive =
      solver().solve(two_by_two_anstreicher(-2), options);
  ASSERT_EQ(noncopositive.status,
            solve_status::positive_objective_found);
  EXPECT_GT(noncopositive.objective.sign(), 0);
  EXPECT_FALSE(noncopositive.point.empty());
}

} // namespace
