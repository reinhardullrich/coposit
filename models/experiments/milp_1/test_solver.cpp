#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>

using namespace coposit;

namespace {

matrix_integer symmetric_matrix(size_t dimension,
                                std::initializer_list<slong> upper_triangle) {
  matrix_integer matrix(dimension, dimension);
  auto value = upper_triangle.begin();
  for (size_t row = 0; row < dimension; ++row) {
    for (size_t column = row; column < dimension; ++column) {
      matrix(row, column) = integer(*value++);
      matrix(column, row) = matrix(row, column);
    }
  }
  return matrix;
}

TEST(Milp1ModelTest, ClassifiesTheExactSimplexMinimum) {
  const auto strict = model::classify(symmetric_matrix(2, {2, -1, 2}));
  EXPECT_TRUE(strict.is_copositive);
  EXPECT_TRUE(strict.is_strictly_copositive);

  const auto boundary = model::classify(symmetric_matrix(2, {1, -1, 1}));
  EXPECT_TRUE(boundary.is_copositive);
  EXPECT_FALSE(boundary.is_strictly_copositive);

  const auto noncopositive = model::classify(symmetric_matrix(2, {1, -2, 1}));
  EXPECT_FALSE(noncopositive.is_copositive);
  EXPECT_FALSE(noncopositive.is_strictly_copositive);
}

TEST(Milp1ModelTest, HandlesOrderOneWithoutTheThetaTwoShortcut) {
  const auto positive = model::classify(symmetric_matrix(1, {1}));
  EXPECT_TRUE(positive.is_copositive);
  EXPECT_TRUE(positive.is_strictly_copositive);

  const auto zero = model::classify(symmetric_matrix(1, {0}));
  EXPECT_TRUE(zero.is_copositive);
  EXPECT_FALSE(zero.is_strictly_copositive);

  const auto negative = model::classify(symmetric_matrix(1, {-1}));
  EXPECT_FALSE(negative.is_copositive);
  EXPECT_FALSE(negative.is_strictly_copositive);
}

TEST(Milp1ModelTest, SupportsBothIndividualPredicates) {
  const matrix_integer boundary = symmetric_matrix(2, {1, -1, 1});
  EXPECT_TRUE(model::solve(boundary, model::copositivity_mode::copositive));
  EXPECT_FALSE(
      model::solve(boundary, model::copositivity_mode::strictly_copositive));
}

TEST(Milp1ModelTest, MatchesTheExactOrderTwoCriterion) {
  for (slong diagonal_1 = -2; diagonal_1 <= 2; ++diagonal_1) {
    for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
      for (slong diagonal_2 = -2; diagonal_2 <= 2; ++diagonal_2) {
        const auto result = model::classify(
            symmetric_matrix(2, {diagonal_1, off_diagonal, diagonal_2}));
        const slong square = off_diagonal * off_diagonal;
        const slong diagonal_product = diagonal_1 * diagonal_2;
        const bool copositive =
            diagonal_1 >= 0 && diagonal_2 >= 0 &&
            (off_diagonal >= 0 || square <= diagonal_product);
        const bool strictly_copositive =
            diagonal_1 > 0 && diagonal_2 > 0 &&
            (off_diagonal >= 0 || square < diagonal_product);
        EXPECT_EQ(result.is_copositive, copositive);
        EXPECT_EQ(result.is_strictly_copositive, strictly_copositive);
      }
    }
  }
}

} // namespace
