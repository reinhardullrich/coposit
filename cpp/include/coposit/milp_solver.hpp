#pragma once

#include <coposit/timeout.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit {

struct maximum_halfspaces_milp_result {
    std::vector<double> point;
    // Number of satisfied rows for the unit-weight constructor; total satisfied weight otherwise.
    size_t satisfied = 0;
    size_t nodes = 0;
    bool optimal = false;
    bool root_relaxation_solved = false;
};

// Maximizes the total weight of rows r satisfying r*x >= 0 over a nonnegative or lower-bounded simplex.
// This is a numerical proposal generator; callers must verify every accepted result exactly.
class maximum_halfspaces_milp_solver {
public:
    maximum_halfspaces_milp_solver(const std::vector<std::vector<double>>& rows, double positive_floor, size_t incumbent,
                                   size_t node_limit, std::chrono::steady_clock::time_point deadline,
                                   size_t objective_ceiling = std::numeric_limits<size_t>::max())
        : maximum_halfspaces_milp_solver(rows, std::vector<size_t>(rows.size(), 1), positive_floor, incumbent, node_limit, deadline,
                                         objective_ceiling)
    {
    }

    maximum_halfspaces_milp_solver(const std::vector<std::vector<double>>& rows, std::vector<size_t> row_weights,
                                   double positive_floor, size_t incumbent, size_t node_limit,
                                   std::chrono::steady_clock::time_point deadline,
                                   size_t objective_ceiling = std::numeric_limits<size_t>::max())
        : rows_(rows)
        , row_weights_(std::move(row_weights))
        , dimension_(rows.empty() ? 0 : rows.front().size())
        , positive_floor_(positive_floor)
        , simplex_scale_(1.0 - positive_floor * static_cast<double>(dimension_))
        , incumbent_(incumbent)
        , node_limit_(node_limit)
        , deadline_(deadline)
        , objective_ceiling_(objective_ceiling)
    {
        if (dimension_ == 0 || positive_floor_ < 0.0 || !(simplex_scale_ > 0.0))
            throw std::invalid_argument("invalid simplex");
        if (row_weights_.size() != rows_.size() ||
            std::any_of(row_weights_.begin(), row_weights_.end(), [](size_t weight) { return weight == 0; }))
            throw std::invalid_argument("invalid halfspace weights");
        for (const auto& row : rows_)
            if (row.size() != dimension_) throw std::invalid_argument("ragged halfspace matrix");
        classify_rows();
        fixed_.assign(variable_rows_.size(), -1);
    }

    maximum_halfspaces_milp_result solve()
    {
        result_.satisfied = incumbent_;
        result_.optimal = search(0, 0);
        return result_;
    }

private:
    class simplex {
    public:
        enum class status { optimal, infeasible, unbounded, interrupted };

        simplex(size_t rows, size_t columns,
                std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max())
            : rows_(rows)
            , columns_(columns)
            , stride_(columns + 2)
            , tableau_(checked_size(rows, columns), 0.0)
            , basic_(rows)
            , nonbasic_(columns + 1)
            , deadline_(deadline)
            , timeout_check_stride_(std::clamp<size_t>(1000000 / tableau_.size(), 1, 64))
        {
            for (size_t row = 0; row < rows_; ++row) {
                basic_[row] = static_cast<int>(columns_ + row);
                at(row, columns_) = -1.0;
            }
            for (size_t column = 0; column < columns_; ++column) nonbasic_[column] = static_cast<int>(column);
            nonbasic_[columns_] = -1;
            at(rows_ + 1, columns_) = 1.0;
        }

        void set_coefficient(size_t row, size_t column, double value) { at(row, column) = value; }
        void set_rhs(size_t row, double value) { at(row, columns_ + 1) = value; }
        void set_objective(size_t column, double value) { at(rows_, column) = -value; }

        status solve(std::vector<double>& solution)
        {
            size_t pivot_row = 0;
            for (size_t row = 1; row < rows_; ++row)
                if (at(row, columns_ + 1) < at(pivot_row, columns_ + 1)) pivot_row = row;
            if (at(pivot_row, columns_ + 1) < -epsilon) {
                pivot(pivot_row, columns_);
                if (!run(true)) return interrupted_ ? status::interrupted : status::infeasible;
                if (at(rows_ + 1, columns_ + 1) < -epsilon || std::abs(at(rows_ + 1, columns_ + 1)) > epsilon)
                    return status::infeasible;
                for (size_t row = 0; row < rows_; ++row) {
                    if (basic_[row] != -1) continue;
                    size_t column = 0;
                    for (size_t candidate = 1; candidate <= columns_; ++candidate)
                        if (std::abs(at(row, candidate)) > std::abs(at(row, column))) column = candidate;
                    if (std::abs(at(row, column)) > epsilon) pivot(row, column);
                }
            }
            if (!run(false)) return interrupted_ ? status::interrupted : status::unbounded;
            solution.assign(columns_, 0.0);
            for (size_t row = 0; row < rows_; ++row)
                if (basic_[row] >= 0 && static_cast<size_t>(basic_[row]) < columns_)
                    solution[static_cast<size_t>(basic_[row])] = at(row, columns_ + 1);
            return status::optimal;
        }

    private:
        static constexpr double epsilon = 1e-9;

        static size_t checked_size(size_t rows, size_t columns)
        {
            if (rows == 0 || columns == 0) throw std::invalid_argument("simplex tableau must be nonempty");
            if (columns > static_cast<size_t>(std::numeric_limits<int>::max()) ||
                rows > static_cast<size_t>(std::numeric_limits<int>::max()) - columns ||
                rows + 2 > std::numeric_limits<size_t>::max() / (columns + 2))
                throw std::overflow_error("simplex tableau is too large");
            return (rows + 2) * (columns + 2);
        }

        double& at(size_t row, size_t column) { return tableau_[row * stride_ + column]; }
        double at(size_t row, size_t column) const { return tableau_[row * stride_ + column]; }

        void pivot(size_t pivot_row, size_t pivot_column)
        {
            ++pivots_;
            const double inverse = 1.0 / at(pivot_row, pivot_column);
            const double* pivot_data = tableau_.data() + pivot_row * stride_;
            for (size_t row = 0; row < rows_ + 2; ++row) {
                if (row == pivot_row) continue;
                double* row_data = tableau_.data() + row * stride_;
                const double factor = row_data[pivot_column] * inverse;
                for (size_t column = 0; column < stride_; ++column)
                    if (column != pivot_column) row_data[column] -= pivot_data[column] * factor;
            }
            double* mutable_pivot_data = tableau_.data() + pivot_row * stride_;
            for (size_t column = 0; column < stride_; ++column)
                if (column != pivot_column) mutable_pivot_data[column] *= inverse;
            for (size_t row = 0; row < rows_ + 2; ++row)
                if (row != pivot_row) tableau_[row * stride_ + pivot_column] *= -inverse;
            mutable_pivot_data[pivot_column] = inverse;
            std::swap(basic_[pivot_row], nonbasic_[pivot_column]);
        }

        bool run(bool phase_one)
        {
            const size_t objective_row = phase_one ? rows_ + 1 : rows_;
            while (true) {
                if (pivots_ % timeout_check_stride_ == 0 &&
                    (timeout_pending() || std::chrono::steady_clock::now() >= deadline_)) {
                    interrupted_ = true;
                    return false;
                }
                size_t pivot_column = columns_ + 1;
                for (size_t column = 0; column <= columns_; ++column) {
                    if (!phase_one && nonbasic_[column] == -1) continue;
                    if (pivot_column == columns_ + 1 || at(objective_row, column) < at(objective_row, pivot_column) - epsilon ||
                        (std::abs(at(objective_row, column) - at(objective_row, pivot_column)) <= epsilon &&
                         nonbasic_[column] < nonbasic_[pivot_column]))
                        pivot_column = column;
                }
                if (pivot_column == columns_ + 1 || at(objective_row, pivot_column) >= -epsilon) return true;
                size_t pivot_row = rows_;
                for (size_t row = 0; row < rows_; ++row) {
                    if (at(row, pivot_column) <= epsilon) continue;
                    if (pivot_row == rows_) {
                        pivot_row = row;
                        continue;
                    }
                    const double ratio = at(row, columns_ + 1) / at(row, pivot_column);
                    const double current = at(pivot_row, columns_ + 1) / at(pivot_row, pivot_column);
                    if (ratio < current - epsilon ||
                        (std::abs(ratio - current) <= epsilon && basic_[row] < basic_[pivot_row]))
                        pivot_row = row;
                }
                if (pivot_row == rows_) return false;
                pivot(pivot_row, pivot_column);
            }
        }

        size_t rows_;
        size_t columns_;
        size_t stride_;
        std::vector<double> tableau_;
        std::vector<int> basic_;
        std::vector<int> nonbasic_;
        std::chrono::steady_clock::time_point deadline_;
        size_t timeout_check_stride_;
        size_t pivots_ = 0;
        bool interrupted_ = false;
    };

    struct variable_row {
        size_t source = 0;
        size_t weight = 0;
        double minimum = 0.0;
        double offset = 0.0;
    };

    void classify_rows()
    {
        for (size_t source = 0; source < rows_.size(); ++source) {
            const auto [minimum_entry, maximum_entry] = std::minmax_element(rows_[source].begin(), rows_[source].end());
            const double sum = std::accumulate(rows_[source].begin(), rows_[source].end(), 0.0);
            const double minimum = positive_floor_ * sum + simplex_scale_ * *minimum_entry;
            const double maximum = positive_floor_ * sum + simplex_scale_ * *maximum_entry;
            if (minimum >= -feasibility_epsilon) always_satisfied_ += row_weights_[source];
            else if (maximum >= -feasibility_epsilon) {
                variable_rows_.push_back({source, row_weights_[source], minimum, positive_floor_ * sum});
                variable_weight_ += row_weights_[source];
            }
        }
    }

    size_t score_point(const std::vector<double>& point) const
    {
        size_t count = always_satisfied_;
        for (const auto& descriptor : variable_rows_) {
            const auto& row = rows_[descriptor.source];
            double product = 0.0;
            for (size_t column = 0; column < dimension_; ++column) product += row[column] * point[column];
            if (product >= -feasibility_epsilon) count += descriptor.weight;
        }
        return count;
    }

    bool search(size_t fixed_one_count, size_t fixed_zero_count, size_t fixed_one_weight = 0, size_t fixed_zero_weight = 0)
    {
        if (result_.satisfied >= objective_ceiling_) return true;
        if (result_.nodes == node_limit_ || timeout_pending() || std::chrono::steady_clock::now() >= deadline_) return false;
        ++result_.nodes;

        if (always_satisfied_ + variable_weight_ - fixed_zero_weight <= result_.satisfied) return true;

        const size_t unfixed_count = variable_rows_.size() - fixed_one_count - fixed_zero_count;
        const size_t variable_count = dimension_ + unfixed_count;
        if (unfixed_count > (std::numeric_limits<size_t>::max() - fixed_one_count - 2) / 2) return false;
        const size_t constraint_count = unfixed_count * 2 + fixed_one_count + 2;
        if (constraint_count + 2 > maximum_tableau_cells / (variable_count + 2)) return false;

        std::vector<size_t> unfixed_rows;
        unfixed_rows.reserve(unfixed_count);
        for (size_t row = 0; row < fixed_.size(); ++row)
            if (fixed_[row] < 0) unfixed_rows.push_back(row);

        double relaxation = static_cast<double>(always_satisfied_ + fixed_one_weight);
        size_t branch = variable_rows_.size();
        std::vector<double> solution;
        {
            simplex lp(constraint_count, variable_count, deadline_);
            for (size_t local_row = 0; local_row < unfixed_rows.size(); ++local_row) {
                const auto& descriptor = variable_rows_[unfixed_rows[local_row]];
                const auto& coefficients = rows_[descriptor.source];
                const double big_m = -descriptor.minimum;
                for (size_t column = 0; column < dimension_; ++column)
                    lp.set_coefficient(local_row, column, -simplex_scale_ * coefficients[column]);
                lp.set_coefficient(local_row, dimension_ + local_row, big_m);
                lp.set_rhs(local_row, big_m + descriptor.offset);
                lp.set_coefficient(unfixed_count + local_row, dimension_ + local_row, 1.0);
                lp.set_rhs(unfixed_count + local_row, 1.0);
                lp.set_objective(dimension_ + local_row, static_cast<double>(descriptor.weight));
            }

            size_t next_constraint = unfixed_count * 2;
            for (size_t row = 0; row < fixed_.size(); ++row) {
                if (fixed_[row] != 1) continue;
                const auto& descriptor = variable_rows_[row];
                const auto& coefficients = rows_[descriptor.source];
                for (size_t column = 0; column < dimension_; ++column)
                    lp.set_coefficient(next_constraint, column, -simplex_scale_ * coefficients[column]);
                lp.set_rhs(next_constraint, descriptor.offset);
                ++next_constraint;
            }
            for (size_t column = 0; column < dimension_; ++column) {
                lp.set_coefficient(next_constraint, column, 1.0);
                lp.set_coefficient(next_constraint + 1, column, -1.0);
            }
            lp.set_rhs(next_constraint, 1.0);
            lp.set_rhs(next_constraint + 1, -1.0);

            const simplex::status status = lp.solve(solution);
            if (status == simplex::status::interrupted) return false;
            if (status == simplex::status::infeasible) return true;
            if (status != simplex::status::optimal) return false;
            if (fixed_one_count == 0 && fixed_zero_count == 0) result_.root_relaxation_solved = true;
        }

        std::vector<double> point(dimension_);
        for (size_t column = 0; column < dimension_; ++column)
            point[column] = positive_floor_ + simplex_scale_ * solution[column];
        const size_t score = score_point(point);
        if (score > result_.satisfied) {
            result_.satisfied = score;
            result_.point = std::move(point);
        }
        if (result_.satisfied >= objective_ceiling_) return true;

        double closest = std::numeric_limits<double>::infinity();
        for (size_t local_row = 0; local_row < unfixed_rows.size(); ++local_row) {
            const double value = solution[dimension_ + local_row];
            relaxation += static_cast<double>(variable_rows_[unfixed_rows[local_row]].weight) * value;
            if (value <= integrality_epsilon || value >= 1.0 - integrality_epsilon) continue;
            const double distance = std::abs(value - 0.5);
            if (distance < closest) {
                closest = distance;
                branch = unfixed_rows[local_row];
            }
        }

        // The objective is integral. A small upward tolerance protects an integer LP bound from floating underestimation.
        if (static_cast<size_t>(std::floor(relaxation + bound_epsilon)) <= result_.satisfied ||
            branch == variable_rows_.size())
            return true;
        if (result_.nodes == node_limit_) return false;

        fixed_[branch] = 1;
        const bool one_complete = search(fixed_one_count + 1, fixed_zero_count,
                                         fixed_one_weight + variable_rows_[branch].weight, fixed_zero_weight);
        fixed_[branch] = 0;
        const bool zero_complete = search(fixed_one_count, fixed_zero_count + 1, fixed_one_weight,
                                          fixed_zero_weight + variable_rows_[branch].weight);
        fixed_[branch] = -1;
        return one_complete && zero_complete;
    }

    static constexpr double feasibility_epsilon = 1e-9;
    static constexpr double integrality_epsilon = 1e-9;
    static constexpr double bound_epsilon = 1e-6;
    static constexpr size_t maximum_tableau_cells = size_t{8} * 1024 * 1024;
    const std::vector<std::vector<double>>& rows_;
    std::vector<size_t> row_weights_;
    size_t dimension_;
    double positive_floor_;
    double simplex_scale_;
    size_t incumbent_;
    size_t node_limit_;
    std::chrono::steady_clock::time_point deadline_;
    size_t objective_ceiling_;
    size_t always_satisfied_ = 0;
    size_t variable_weight_ = 0;
    std::vector<variable_row> variable_rows_;
    std::vector<int8_t> fixed_;
    maximum_halfspaces_milp_result result_;
};

} // namespace coposit
