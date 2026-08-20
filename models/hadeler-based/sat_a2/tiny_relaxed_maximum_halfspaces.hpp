#pragma once

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

namespace coposit::model::detail {

class tiny_simplex {
public:
    enum class status { optimal, infeasible, unbounded, interrupted };

    tiny_simplex(size_t rows, size_t columns,
                 std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max())
        : rows_(rows)
        , columns_(columns)
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

    double& at(size_t row, size_t column) { return tableau_[row * (columns_ + 2) + column]; }
    double at(size_t row, size_t column) const { return tableau_[row * (columns_ + 2) + column]; }

    void pivot(size_t pivot_row, size_t pivot_column)
    {
        ++pivots_;
        const double inverse = 1.0 / at(pivot_row, pivot_column);
        for (size_t row = 0; row < rows_ + 2; ++row) {
            if (row == pivot_row) continue;
            for (size_t column = 0; column < columns_ + 2; ++column) {
                if (column == pivot_column) continue;
                at(row, column) -= at(pivot_row, column) * at(row, pivot_column) * inverse;
            }
        }
        for (size_t column = 0; column < columns_ + 2; ++column)
            if (column != pivot_column) at(pivot_row, column) *= inverse;
        for (size_t row = 0; row < rows_ + 2; ++row)
            if (row != pivot_row) at(row, pivot_column) *= -inverse;
        at(pivot_row, pivot_column) = inverse;
        std::swap(basic_[pivot_row], nonbasic_[pivot_column]);
    }

    bool run(bool phase_one)
    {
        const size_t objective_row = phase_one ? rows_ + 1 : rows_;
        while (true) {
            if (pivots_ % timeout_check_stride_ == 0 && std::chrono::steady_clock::now() >= deadline_) {
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
    std::vector<double> tableau_;
    std::vector<int> basic_;
    std::vector<int> nonbasic_;
    std::chrono::steady_clock::time_point deadline_;
    size_t timeout_check_stride_;
    size_t pivots_ = 0;
    bool interrupted_ = false;
};

struct relaxed_maximum_halfspaces_result {
    std::vector<double> point;
    std::vector<double> row_values;
    double upper_bound = 0.0;
    bool optimal = false;
    bool infeasible = false;
};

class tiny_relaxed_maximum_halfspaces {
public:
    tiny_relaxed_maximum_halfspaces(const std::vector<std::vector<double>>& rows, double positive_floor,
                                    const std::vector<bool>& forced, std::chrono::steady_clock::time_point deadline)
        : rows_(rows)
        , dimension_(rows.empty() ? 0 : rows.front().size())
        , positive_floor_(positive_floor)
        , simplex_scale_(1.0 - positive_floor * static_cast<double>(dimension_))
        , forced_(forced)
        , deadline_(deadline)
    {
        if (dimension_ == 0 || !(simplex_scale_ > 0.0)) throw std::invalid_argument("invalid positive simplex");
        if (!forced_.empty() && forced_.size() != rows_.size()) throw std::invalid_argument("invalid forced-row vector");
        for (const auto& row : rows_)
            if (row.size() != dimension_) throw std::invalid_argument("ragged halfspace matrix");
        classify_rows();
    }

    relaxed_maximum_halfspaces_result solve() const
    {
        relaxed_maximum_halfspaces_result result;
        result.row_values.assign(rows_.size(), 0.0);
        for (const size_t source : always_satisfied_) result.row_values[source] = 1.0;
        if (forced_impossible_) {
            result.infeasible = true;
            return result;
        }

        size_t forced_count = 0;
        for (const auto& row : variable_rows_) forced_count += is_forced(row.source);
        constexpr size_t maximum_size = std::numeric_limits<size_t>::max();
        if (variable_rows_.size() > maximum_size - dimension_ || forced_count > maximum_size - 4 ||
            variable_rows_.size() > (maximum_size - forced_count - 4) / 2)
            return result;
        const size_t variable_count = dimension_ + variable_rows_.size();
        const size_t constraint_count = variable_rows_.size() * 2 + forced_count + 2;
        if (variable_count > maximum_size - 2 || constraint_count + 2 > maximum_tableau_cells / (variable_count + 2)) return result;

        std::vector<double> solution;
        tiny_simplex lp(constraint_count, variable_count, deadline_);
        for (size_t row = 0; row < variable_rows_.size(); ++row) {
            const auto& descriptor = variable_rows_[row];
            const auto& coefficients = rows_[descriptor.source];
            const double big_m = -descriptor.minimum;
            for (size_t column = 0; column < dimension_; ++column)
                lp.set_coefficient(row, column, -simplex_scale_ * coefficients[column]);
            lp.set_coefficient(row, dimension_ + row, big_m);
            lp.set_rhs(row, big_m + descriptor.offset);
            lp.set_coefficient(variable_rows_.size() + row, dimension_ + row, 1.0);
            lp.set_rhs(variable_rows_.size() + row, 1.0);
            lp.set_objective(dimension_ + row, 1.0);
        }

        size_t next_constraint = variable_rows_.size() * 2;
        for (size_t row = 0; row < variable_rows_.size(); ++row) {
            if (!is_forced(variable_rows_[row].source)) continue;
            lp.set_coefficient(next_constraint, dimension_ + row, -1.0);
            lp.set_rhs(next_constraint, -1.0);
            ++next_constraint;
        }
        for (size_t column = 0; column < dimension_; ++column) {
            lp.set_coefficient(next_constraint, column, 1.0);
            lp.set_coefficient(next_constraint + 1, column, -1.0);
        }
        lp.set_rhs(next_constraint, 1.0);
        lp.set_rhs(next_constraint + 1, -1.0);

        const tiny_simplex::status status = lp.solve(solution);
        result.infeasible = status == tiny_simplex::status::infeasible;
        if (status != tiny_simplex::status::optimal) return result;
        if (solution.size() != variable_count ||
            !std::all_of(solution.begin(), solution.end(), [](double value) { return std::isfinite(value); }))
            return result;

        result.point.resize(dimension_);
        for (size_t column = 0; column < dimension_; ++column)
            result.point[column] = positive_floor_ + simplex_scale_ * solution[column];
        result.upper_bound = static_cast<double>(always_satisfied_.size());
        for (size_t row = 0; row < variable_rows_.size(); ++row) {
            const double value = solution[dimension_ + row];
            result.row_values[variable_rows_[row].source] = value;
            result.upper_bound += value;
        }
        result.optimal = true;
        return result;
    }

private:
    struct variable_row {
        size_t source = 0;
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
            if (minimum >= -feasibility_epsilon) always_satisfied_.push_back(source);
            else if (maximum >= -feasibility_epsilon)
                variable_rows_.push_back({source, minimum, positive_floor_ * sum});
            else if (is_forced(source))
                forced_impossible_ = true;
        }
    }

    bool is_forced(size_t source) const noexcept { return !forced_.empty() && forced_[source]; }

    static constexpr double feasibility_epsilon = 1e-9;
    static constexpr size_t maximum_tableau_cells = size_t{8} * 1024 * 1024;
    const std::vector<std::vector<double>>& rows_;
    size_t dimension_;
    double positive_floor_;
    double simplex_scale_;
    const std::vector<bool>& forced_;
    std::chrono::steady_clock::time_point deadline_;
    std::vector<size_t> always_satisfied_;
    std::vector<variable_row> variable_rows_;
    bool forced_impossible_ = false;
};

} // namespace coposit::model::detail
