#include <coposit/diagnostics.hpp>
#include <coposit/improved_nbc_upward_supports.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t improved_nbc_b8_pair_upward_count_for_testing() noexcept;
size_t improved_nbc_b8_support_upward_count_for_testing() noexcept;
size_t improved_nbc_b8_optimized_certificate_count_for_testing() noexcept;
bool improved_nbc_b8_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool improved_nbc_b8_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             support &lower, support &upper);
size_t improved_nbc_b8_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals);
} // namespace coposit::model

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

support support_from_mask(size_t dimension, uint64_t mask) {
  support result(dimension);
  for (size_t index = 0; index < dimension; ++index)
    if ((mask & (uint64_t{1} << index)) != 0)
      result.set(index);
  return result;
}

uint64_t support_mask(const std::vector<size_t> &indices) {
  uint64_t result = 0;
  for (const size_t index : indices)
    result |= uint64_t{1} << index;
  return result;
}

bool interval_covers(uint64_t mask, uint64_t lower, uint64_t upper) {
  return (lower & ~mask) == 0 && (mask & ~upper) == 0;
}

TEST(ImprovedNbcB8Test, ResumesOneModelCallsWithoutRepeatingSupports) {
  improved_nbc_upward_supports supports(5);
  supports.start_cardinality(2);
  std::vector<bool> seen(uint64_t{1} << 5, false);
  std::vector<size_t> indices;
  size_t count = 0;
  while (supports.take_first(indices)) {
    ASSERT_EQ(indices.size(), 2U);
    const uint64_t mask = support_mask(indices);
    ASSERT_FALSE(seen[mask]);
    seen[mask] = true;
    ++count;
  }
  EXPECT_EQ(count, 10U);
}

TEST(ImprovedNbcB8Test, LatchesCollectiveRootInconsistencyAcrossCalls) {
  improved_nbc_upward_supports supports(3);
  supports.add_interval(support_from_mask(3, 0b000),
                        support_from_mask(3, 0b101));
  supports.start_cardinality(3);

  std::vector<size_t> indices;
  ASSERT_TRUE(supports.take_first(indices));
  EXPECT_EQ(support_mask(indices), 0b111U);

  supports.add_interval(support_from_mask(3, 0b010),
                        support_from_mask(3, 0b111));
  EXPECT_FALSE(supports.take_first(indices));
  EXPECT_TRUE(supports.all_future_covered());
}

TEST(ImprovedNbcB8Test, ExhaustivelyMatchesSingleIntervalCoverage) {
  for (size_t dimension = 1; dimension <= 5; ++dimension) {
    const uint64_t end = uint64_t{1} << dimension;
    for (uint64_t lower = 0; lower < end; ++lower) {
      for (uint64_t upper = lower; upper < end; ++upper) {
        if ((lower & ~upper) != 0)
          continue;
        for (size_t cardinality = 1; cardinality <= dimension; ++cardinality) {
          improved_nbc_upward_supports supports(dimension);
          supports.add_interval(support_from_mask(dimension, lower),
                                support_from_mask(dimension, upper));
          supports.start_cardinality(cardinality);
          std::vector<bool> seen(end, false);
          std::vector<size_t> indices;
          while (supports.take_first(indices)) {
            const uint64_t mask = support_mask(indices);
            ASSERT_EQ(indices.size(), cardinality);
            ASSERT_FALSE(interval_covers(mask, lower, upper));
            ASSERT_FALSE(seen[mask]);
            seen[mask] = true;
          }
          for (uint64_t mask = 1; mask < end; ++mask) {
            if (static_cast<size_t>(__builtin_popcountll(mask)) != cardinality)
              continue;
            EXPECT_EQ(seen[mask], !interval_covers(mask, lower, upper));
          }
        }
      }
    }
  }
}

TEST(ImprovedNbcB8Test, ExhaustivelyMatchesIntervalsAddedBetweenModelCalls) {
  constexpr size_t dimension = 3;
  constexpr uint64_t end = uint64_t{1} << dimension;
  for (uint64_t first_lower = 0; first_lower < end; ++first_lower) {
    for (uint64_t first_upper = first_lower; first_upper < end; ++first_upper) {
      if ((first_lower & ~first_upper) != 0)
        continue;
      for (uint64_t second_lower = 0; second_lower < end; ++second_lower) {
        for (uint64_t second_upper = second_lower; second_upper < end;
             ++second_upper) {
          if ((second_lower & ~second_upper) != 0)
            continue;
          for (size_t cardinality = 1; cardinality <= dimension;
               ++cardinality) {
            improved_nbc_upward_supports supports(dimension);
            supports.add_interval(support_from_mask(dimension, first_lower),
                                  support_from_mask(dimension, first_upper));
            supports.start_cardinality(cardinality);
            std::vector<bool> seen(end, false);
            std::vector<size_t> indices;
            if (supports.take_first(indices))
              seen[support_mask(indices)] = true;

            supports.add_interval(support_from_mask(dimension, second_lower),
                                  support_from_mask(dimension, second_upper));
            while (supports.take_first(indices)) {
              const uint64_t mask = support_mask(indices);
              ASSERT_EQ(indices.size(), cardinality);
              ASSERT_FALSE(interval_covers(mask, first_lower, first_upper));
              ASSERT_FALSE(interval_covers(mask, second_lower, second_upper));
              ASSERT_FALSE(seen[mask]);
              seen[mask] = true;
            }
            for (uint64_t mask = 1; mask < end; ++mask) {
              if (static_cast<size_t>(__builtin_popcountll(mask)) !=
                  cardinality)
                continue;
              if (!interval_covers(mask, first_lower, first_upper) &&
                  !interval_covers(mask, second_lower, second_upper))
                EXPECT_TRUE(seen[mask]);
            }
          }
        }
      }
    }
  }
}

TEST(ImprovedNbcB8Test, TraversesOnlyTheAscendingLowFrontier) {
  improved_nbc_b8_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_b8_pair_upward_count_for_testing(), 0U);
  EXPECT_EQ(model::improved_nbc_b8_support_upward_count_for_testing(), 0U);
  ASSERT_FALSE(improved_nbc_b8_diagnostics::events.empty());
  EXPECT_TRUE(std::all_of(
      improved_nbc_b8_diagnostics::events.begin(),
      improved_nbc_b8_diagnostics::events.end(), [](const auto &event) {
        return event.name != "process_high" && event.name != "downward";
      }));
  EXPECT_NE(std::find(improved_nbc_b8_diagnostics::events.begin(),
                      improved_nbc_b8_diagnostics::events.end(),
                      improved_nbc_b8_diagnostics::event{"dickinson", 1}),
            improved_nbc_b8_diagnostics::events.end());
}

TEST(ImprovedNbcB8Test, RecordsChronologicalCertificatesForTheRenderer) {
  std::ostringstream ignored_output;
  {
    diagnostics::reporter reporter(false, ignored_output, true, true);
    const auto classification =
        model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));
    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
  }

  const std::string history = diagnostics::detail::load_diagnostics();
  const size_t dickinson =
      history.find("event=certificate sequence=1 model=improved_nbc_b8 n=3 "
                   "frontier=low kind=dickinson source=[");
  ASSERT_NE(dickinson, std::string::npos) << history;
  EXPECT_NE(history.find("coverage=interval lower=[", dickinson),
            std::string::npos);
  EXPECT_EQ(history.find("frontier=high"), std::string::npos);
  EXPECT_EQ(history.find("coverage=downward"), std::string::npos);
}

TEST(ImprovedNbcB8Test, BuildsAnExactDickinsonIntervalOnTheLowPath) {
  matrix_integer identity;
  identity.set_identity(3);
  support lower(3);
  support upper(3);

  EXPECT_TRUE(model::improved_nbc_b8_certificate_for_testing(identity, {0},
                                                             lower, upper));
  EXPECT_TRUE(lower.contains(0));
  EXPECT_FALSE(lower.contains(1));
  EXPECT_FALSE(lower.contains(2));
  EXPECT_TRUE(upper.contains(0));
  EXPECT_TRUE(upper.contains(1));
  EXPECT_TRUE(upper.contains(2));
}

TEST(ImprovedNbcB8Test, RetainsTheExactHalfspaceRayOptimization) {
  const matrix_integer matrix =
      symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
  bool improved = false;
  for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
    std::vector<size_t> indices;
    for (size_t index = 0; index < 4; ++index)
      if ((mask & (uint64_t{1} << index)) != 0)
        indices.push_back(index);
    (void)model::improved_nbc_b8_check_support_for_testing(matrix, indices);
    improved =
        model::improved_nbc_b8_optimized_certificate_count_for_testing() > 0;
  }
  EXPECT_TRUE(improved);
}

TEST(ImprovedNbcB8Test, StoresDickinsonIntervalsInTheBufferedNbcState) {
  const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011},
                                                             {0b100, 0b110}};
  EXPECT_EQ(model::improved_nbc_b8_uncovered_count(3, 1, intervals), 1U);
  EXPECT_EQ(model::improved_nbc_b8_uncovered_count(3, 2, intervals), 1U);

  const std::vector<std::pair<uint64_t, uint64_t>> padded_network_interval{
      {0b00001, 0b01111}};
  EXPECT_EQ(
      model::improved_nbc_b8_uncovered_count(5, 2, padded_network_interval),
      7U);

  const std::vector<std::pair<uint64_t, uint64_t>> singular_pair_interval{
      {0b00011, 0b01111}};
  EXPECT_EQ(
      model::improved_nbc_b8_uncovered_count(5, 2, singular_pair_interval), 9U);
}

TEST(ImprovedNbcB8Test, PreservesExactCombinedClassifications) {
  const auto strict = model::classify(symmetric_matrix(2, {2, -1, 2}));
  EXPECT_TRUE(strict.is_copositive);
  EXPECT_TRUE(strict.is_strictly_copositive);

  const auto boundary = model::classify(symmetric_matrix(2, {1, -1, 1}));
  EXPECT_TRUE(boundary.is_copositive);
  EXPECT_FALSE(boundary.is_strictly_copositive);

  const auto negative = model::classify(symmetric_matrix(2, {1, -2, 1}));
  EXPECT_FALSE(negative.is_copositive);
  EXPECT_FALSE(negative.is_strictly_copositive);
}

TEST(ImprovedNbcB8Test, DoesNotRepeatSupportsAcrossCompactedCardinalityLayers) {
  const auto classification = model::classify(
      symmetric_matrix(5, {1, 2, 0, -2, 4, 4, 3, 0, -1, 1, 3, 2, 5, -3, 5}));
  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
}

TEST(ImprovedNbcB8Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices) {
  for (slong diagonal_0 = -2; diagonal_0 <= 2; ++diagonal_0) {
    for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
      for (slong diagonal_1 = -2; diagonal_1 <= 2; ++diagonal_1) {
        const auto classification = model::classify(
            symmetric_matrix(2, {diagonal_0, off_diagonal, diagonal_1}));
        const slong determinant =
            diagonal_0 * diagonal_1 - off_diagonal * off_diagonal;
        const bool expected_copositive =
            diagonal_0 >= 0 && diagonal_1 >= 0 &&
            (off_diagonal >= 0 || determinant >= 0);
        const bool expected_strict = diagonal_0 > 0 && diagonal_1 > 0 &&
                                     (off_diagonal >= 0 || determinant > 0);

        SCOPED_TRACE(::testing::Message()
                     << "A=[" << diagonal_0 << ',' << off_diagonal << ';'
                     << diagonal_1 << ']');
        EXPECT_EQ(classification.is_copositive, expected_copositive);
        EXPECT_EQ(classification.is_strictly_copositive, expected_strict);
      }
    }
  }
}

TEST(ImprovedNbcB8Test, MatchesTheIndependentExactThreeByThreeCriterion) {
  for (slong a00 = -1; a00 <= 1; ++a00) {
    for (slong a01 = -1; a01 <= 1; ++a01) {
      for (slong a02 = -1; a02 <= 1; ++a02) {
        for (slong a11 = -1; a11 <= 1; ++a11) {
          for (slong a12 = -1; a12 <= 1; ++a12) {
            for (slong a22 = -1; a22 <= 1; ++a22) {
              const matrix_integer matrix =
                  symmetric_matrix(3, {a00, a01, a02, a11, a12, a22});
              const auto expected = small_copositivity::classify(matrix);
              const auto actual = model::classify(matrix);

              SCOPED_TRACE(::testing::Message()
                           << "A=[" << a00 << ',' << a01 << ',' << a02 << ';'
                           << a11 << ',' << a12 << ';' << a22 << ']');
              EXPECT_EQ(actual.is_copositive, expected.is_copositive);
              EXPECT_EQ(actual.is_strictly_copositive,
                        expected.is_strictly_copositive);
            }
          }
        }
      }
    }
  }
}

TEST(ImprovedNbcB8Test, AppliesPairCurvaturePrepass) {
  const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
  EXPECT_TRUE(pair_excluded.is_copositive);
  EXPECT_TRUE(pair_excluded.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_b8_pair_upward_count_for_testing(), 1U);
}

} // namespace
