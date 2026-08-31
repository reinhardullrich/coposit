#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/parsers/fracessa_matrix_parser.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>

using namespace coposit;

namespace coposit::model {
bool cadical_x1_process_support_for_testing(const matrix_integer &matrix,
                                            const std::vector<size_t> &indices);
bool cadical_x1_ceiling_dickinson_subsumes_upward_for_testing();
bool cadical_x1_process_walk_with_upward_closure_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &seed,
    const std::vector<size_t> &covered_root);
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

bool recorded(std::string_view name) {
  return std::any_of(coposit::cadical_x1_diagnostics::events.begin(),
                     coposit::cadical_x1_diagnostics::events.end(),
                     [&](const auto &event) { return event.name == name; });
}

TEST(CadicalX1Test, ClassifiesStrictCopositivityInOneTraversal) {
  coposit::cadical_x1_diagnostics::clear();
  const auto result = model::classify(symmetric_matrix(3, {2, 0, 0, 3, 0, 4}));

  EXPECT_TRUE(result.is_copositive);
  EXPECT_TRUE(result.is_strictly_copositive);
}

TEST(CadicalX1Test, RetainsBoundedDickinsonIntervals) {
  coposit::cadical_x1_diagnostics::clear();
  const auto matrix = symmetric_matrix(2, {1, -1, 1});

  EXPECT_TRUE(model::cadical_x1_process_support_for_testing(matrix, {0}));
  EXPECT_TRUE(recorded("dickinson_interval"));
}

TEST(CadicalX1Test, CeilingDickinsonRemovesRedundantUpwardClosure) {
  EXPECT_TRUE(
      model::cadical_x1_ceiling_dickinson_subsumes_upward_for_testing());
}

TEST(CadicalX1Test, FindsAnExactNegativeWitnessAfterTheFloatWalk) {
  const auto result = model::classify(symmetric_matrix(2, {1, -2, 1}));

  EXPECT_FALSE(result.is_copositive);
  EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(CadicalX1Test, SupportsBothIndividualPredicates) {
  const auto matrix = symmetric_matrix(2, {1, -1, 1});

  EXPECT_TRUE(model::solve(matrix, model::copositivity_mode::copositive));
  EXPECT_FALSE(
      model::solve(matrix, model::copositivity_mode::strictly_copositive));
}

TEST(CadicalX1Test, KeepsPairCurvaturePruning) {
  coposit::cadical_x1_diagnostics::clear();
  const auto result = model::classify(symmetric_matrix(2, {1, 2, 1}));

  EXPECT_TRUE(result.is_copositive);
  EXPECT_TRUE(result.is_strictly_copositive);
  EXPECT_TRUE(recorded("pair_upward"));
}

TEST(CadicalX1Test, SingletonCeilingCertificatesCanFinishBeforeSat) {
  coposit::cadical_x1_diagnostics::clear();
  std::ostringstream output;
  diagnostics::reporter reporter(false, output, true);

  const auto result = model::classify(symmetric_matrix(3, {2, 1, 1, 2, 1, 2}));

  EXPECT_TRUE(result.is_copositive);
  EXPECT_TRUE(result.is_strictly_copositive);
  EXPECT_TRUE(recorded("dickinson_ceiling"));
  EXPECT_EQ(diagnostics::detail::load_diagnostics().find("event=cadical_seed"),
            std::string::npos);
}

TEST(CadicalX1Test, IgnoresBoundedSingletonIntervalsAndPrefersASparseSeed) {
  std::ostringstream output;
  diagnostics::reporter reporter(false, output, true);

  const auto result = model::classify(symmetric_matrix(3, {3, -1, -1, 3, -1, 3}));
  const std::string history = diagnostics::detail::load_diagnostics();
  const size_t seed = history.find("event=cadical_seed");

  EXPECT_TRUE(result.is_copositive);
  EXPECT_TRUE(result.is_strictly_copositive);
  ASSERT_NE(seed, std::string::npos);
  const size_t end = history.find('\n', seed);
  EXPECT_NE(history.substr(seed, end - seed).find(" cardinality=1 "),
            std::string::npos);
}

TEST(CadicalX1Test, PreservesTheEmittedSeedAcrossWalkVerification) {
  const auto parsed = parsers::fracessa_matrix_parser::parse(
      "9#"
      "18843461267942957920810105202638666044164228350784975569055276078649597,"
      "-11996606529199071157694616990692057266978083828988937769316027263430403"
      ","
      "787922871419865501095804500713444475205565372385795673981046625849597,"
      "2606302200961244794591651417332676134806284771596269741892669937209597,"
      "-591654716266945338363100186838496164310663854289701124379605723590403");
  const auto result = model::classify(parsed.matrix);

  EXPECT_TRUE(result.is_copositive);
  EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(CadicalX1Test, WalkChoosesTheBestUncoveredExtension) {
  coposit::cadical_x1_diagnostics::clear();
  const auto matrix = symmetric_matrix(3, {1, 0, 0, 3, 0, 2});

  EXPECT_TRUE(model::cadical_x1_process_walk_with_upward_closure_for_testing(
      matrix, {0}, {1}));
  EXPECT_TRUE(
      std::find(coposit::cadical_x1_diagnostics::events.begin(),
                coposit::cadical_x1_diagnostics::events.end(),
                coposit::cadical_x1_diagnostics::event{"downward", 2}) !=
      coposit::cadical_x1_diagnostics::events.end());
}

TEST(CadicalX1Test, RecordsTheCadicalSeedImmediately) {
  std::ostringstream output;
  diagnostics::reporter reporter(false, output, true);

  static_cast<void>(model::classify(symmetric_matrix(2, {1, -1, 1})));
  const std::string history = diagnostics::detail::load_diagnostics();

  const size_t seed = history.find("event=cadical_seed");
  ASSERT_NE(seed, std::string::npos);
  const size_t end = history.find('\n', seed);
  EXPECT_NE(history.substr(seed, end - seed).find(" support=["),
            std::string::npos);
}

} // namespace
