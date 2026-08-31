#include <coposit/milp_solver.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <vector>

namespace {

size_t exact_two_dimensional_score(const std::vector<std::vector<double>>& rows, double positive_floor,
                                   const std::vector<size_t>* weights = nullptr)
{
    std::vector<double> candidates{positive_floor, 1.0 - positive_floor};
    for (const auto& row : rows) {
        const double slope = row[0] - row[1];
        if (slope == 0.0) continue;
        const double root = -row[1] / slope;
        if (root > positive_floor && root < 1.0 - positive_floor) candidates.push_back(root);
    }
    std::sort(candidates.begin(), candidates.end());
    const size_t endpoint_count = candidates.size();
    for (size_t index = 1; index < endpoint_count; ++index)
        candidates.push_back((candidates[index - 1] + candidates[index]) / 2.0);

    size_t best = 0;
    for (const double first : candidates) {
        size_t score = 0;
        for (size_t row = 0; row < rows.size(); ++row)
            if (rows[row][0] * first + rows[row][1] * (1.0 - first) >= -1e-9)
                score += weights == nullptr ? 1 : (*weights)[row];
        best = std::max(best, score);
    }
    return best;
}

TEST(MilpSolverTest, MatchesExactTwoDimensionalOptimaAcrossBranchingProblems)
{
    constexpr double positive_floor = 1e-7;
    std::mt19937 generator(1729);
    std::uniform_int_distribution<int> coefficient(-4, 4);
    bool saw_branch = false;
    for (size_t test = 0; test < 500; ++test) {
        std::vector<std::vector<double>> rows(6, std::vector<double>(2));
        for (auto& row : rows)
            for (double& value : row) value = static_cast<double>(coefficient(generator));

        coposit::maximum_halfspaces_milp_solver solver(
            rows, positive_floor, 0, 10000, std::chrono::steady_clock::now() + std::chrono::seconds(1));
        const auto result = solver.solve();
        ASSERT_TRUE(result.optimal);
        EXPECT_EQ(result.satisfied, exact_two_dimensional_score(rows, positive_floor));
        saw_branch |= result.nodes > 1;
    }
    EXPECT_TRUE(saw_branch);
}

TEST(MilpSolverTest, ReportsAnExpiredSearchAsIncomplete)
{
    const std::vector<std::vector<double>> rows{{1.0, -1.0}, {-1.0, 1.0}};
    coposit::maximum_halfspaces_milp_solver solver(
        rows, 1e-7, 0, 10000, std::chrono::steady_clock::now() - std::chrono::seconds(1));
    EXPECT_FALSE(solver.solve().optimal);
}

TEST(MilpSolverTest, MaximizesWeightedHalfspaces)
{
    const std::vector<std::vector<double>> rows{{1.0, -2.0}, {-2.0, 1.0}};
    coposit::maximum_halfspaces_milp_solver solver(
        rows, {10, 1}, 0.0, 0, 10000, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    const auto result = solver.solve();
    ASSERT_TRUE(result.optimal);
    EXPECT_EQ(result.satisfied, 10U);
    ASSERT_EQ(result.point.size(), 2U);
    EXPECT_GE(result.point[0] - 2.0 * result.point[1], -1e-9);
}

TEST(MilpSolverTest, StopsWhenTheKnownObjectiveCeilingIsReached)
{
    std::mt19937 generator(31415);
    std::uniform_int_distribution<int> coefficient(-4, 4);
    bool reduced_search = false;
    for (size_t test = 0; test < 500 && !reduced_search; ++test) {
        std::vector<std::vector<double>> rows(6, std::vector<double>(2));
        for (auto& row : rows)
            for (double& value : row) value = static_cast<double>(coefficient(generator));
        const size_t ceiling = exact_two_dimensional_score(rows, 0.0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        coposit::maximum_halfspaces_milp_solver exhaustive(rows, 0.0, 0, 10000, deadline);
        coposit::maximum_halfspaces_milp_solver ceiling_bounded(rows, 0.0, 0, 10000, deadline, ceiling);

        const auto exhaustive_result = exhaustive.solve();
        const auto ceiling_result = ceiling_bounded.solve();
        ASSERT_TRUE(exhaustive_result.optimal);
        ASSERT_TRUE(ceiling_result.optimal);
        EXPECT_EQ(ceiling_result.satisfied, exhaustive_result.satisfied);
        reduced_search = ceiling_result.nodes < exhaustive_result.nodes;
    }
    EXPECT_TRUE(reduced_search);
}

TEST(MilpSolverTest, CanReturnAfterOnlyTheRootRelaxation)
{
    std::mt19937 generator(16180);
    std::uniform_int_distribution<int> coefficient(-4, 4);
    bool saw_fractional_root = false;
    for (size_t test = 0; test < 500 && !saw_fractional_root; ++test) {
        std::vector<std::vector<double>> rows(6, std::vector<double>(2));
        for (auto& row : rows)
            for (double& value : row) value = static_cast<double>(coefficient(generator));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        coposit::maximum_halfspaces_milp_solver exhaustive(rows, 0.0, 0, 10000, deadline);
        const auto exhaustive_result = exhaustive.solve();
        ASSERT_TRUE(exhaustive_result.optimal);
        if (exhaustive_result.nodes == 1) continue;

        coposit::maximum_halfspaces_milp_solver root_only(rows, 0.0, 0, 1, deadline);
        const auto root_result = root_only.solve();
        EXPECT_FALSE(root_result.optimal);
        EXPECT_TRUE(root_result.root_relaxation_solved);
        EXPECT_EQ(root_result.nodes, 1U);
        saw_fractional_root = true;
    }
    EXPECT_TRUE(saw_fractional_root);
}

TEST(MilpSolverTest, MatchesWeightedTwoDimensionalOptimaAcrossBranchingProblems)
{
    std::mt19937 generator(2718);
    std::uniform_int_distribution<int> coefficient(-4, 4);
    std::uniform_int_distribution<size_t> weight(1, 7);
    bool saw_branch = false;
    for (size_t test = 0; test < 300; ++test) {
        std::vector<std::vector<double>> rows(6, std::vector<double>(2));
        std::vector<size_t> weights(rows.size());
        for (size_t row = 0; row < rows.size(); ++row) {
            for (double& value : rows[row]) value = static_cast<double>(coefficient(generator));
            weights[row] = weight(generator);
        }

        coposit::maximum_halfspaces_milp_solver solver(
            rows, weights, 0.0, 0, 10000, std::chrono::steady_clock::now() + std::chrono::seconds(1));
        const auto result = solver.solve();
        ASSERT_TRUE(result.optimal);
        EXPECT_EQ(result.satisfied, exact_two_dimensional_score(rows, 0.0, &weights));
        saw_branch |= result.nodes > 1;
    }
    EXPECT_TRUE(saw_branch);
}

TEST(MilpSolverTest, CooperativelyStopsForTheGlobalTimeout)
{
    const std::vector<std::vector<double>> rows{{1.0, -1.0}, {-1.0, 1.0}};
    coposit::request_timeout();
    coposit::maximum_halfspaces_milp_solver solver(
        rows, 1e-7, 0, 10000, std::chrono::steady_clock::time_point::max());
    EXPECT_FALSE(solver.solve().optimal);
    coposit::reset_timeout();
}

} // namespace
