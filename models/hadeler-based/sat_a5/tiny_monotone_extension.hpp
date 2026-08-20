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

struct monotone_extension_result {
    std::vector<double> point;
    bool feasible = false;
    bool interrupted = false;
};

class tiny_monotone_extension {
public:
    tiny_monotone_extension(const std::vector<std::vector<double>>& rows, const std::vector<size_t>& required_rows,
                            std::chrono::steady_clock::time_point deadline)
        : rows_(rows)
        , dimension_(rows.empty() ? 0 : rows.front().size())
        , required_rows_(required_rows)
        , deadline_(deadline)
    {
        if (dimension_ == 0) throw std::invalid_argument("empty monotone-extension problem");
        for (const auto& row : rows_)
            if (row.size() != dimension_) throw std::invalid_argument("ragged halfspace matrix");
        for (const size_t row : required_rows_)
            if (row >= rows_.size()) throw std::invalid_argument("invalid required halfspace row");
    }

    monotone_extension_result solve() const
    {
        monotone_extension_result result;
        if (required_rows_.empty()) {
            result.point.assign(dimension_, 1.0);
            result.feasible = true;
            return result;
        }
        if (required_rows_.size() + 2 > maximum_tableau_cells / (dimension_ + 2)) return result;

        tiny_simplex lp(required_rows_.size(), dimension_, deadline_);
        for (size_t constraint = 0; constraint < required_rows_.size(); ++constraint) {
            const auto& coefficients = rows_[required_rows_[constraint]];
            const double sum = std::accumulate(coefficients.begin(), coefficients.end(), 0.0);
            for (size_t column = 0; column < dimension_; ++column)
                lp.set_coefficient(constraint, column, -coefficients[column]);
            // With b = 1 + y and y >= 0, p*b >= 0 is equivalent to -p*y <= p*1.
            lp.set_rhs(constraint, sum);
        }

        std::vector<double> solution;
        const tiny_simplex::status status = lp.solve(solution);
        result.interrupted = status == tiny_simplex::status::interrupted;
        if (status != tiny_simplex::status::optimal) return result;
        if (solution.size() != dimension_ ||
            !std::all_of(solution.begin(), solution.end(), [](double value) { return std::isfinite(value); }))
            return result;

        result.point.resize(dimension_);
        for (size_t column = 0; column < dimension_; ++column)
            result.point[column] = 1.0 + std::max(0.0, solution[column]);
        result.feasible = true;
        return result;
    }

private:
    static constexpr size_t maximum_tableau_cells = size_t{8} * 1024 * 1024;
    const std::vector<std::vector<double>>& rows_;
    size_t dimension_;
    const std::vector<size_t>& required_rows_;
    std::chrono::steady_clock::time_point deadline_;
};

} // namespace coposit::model::detail
