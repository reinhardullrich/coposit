#pragma once

#include <coposit/matrix_scan.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

namespace coposit::copomatrix_precheck {

enum class outcome { accepted, rejected, unresolved };

namespace detail {

struct sparse_ray {
    std::array<size_t, 2> indices{};
    std::array<integer, 2> coefficients{};
    size_t count = 1;
};

struct pivot_choice {
    size_t index;
    size_t children;
};

inline pivot_choice minimum_small_pivot(const matrix_integer& matrix, const matrix_scan_result& scan)
{
    const size_t dimension = matrix.rows();
    assert(scan.dimension == dimension);
    assert(scan.positive_off_diagonal_counts.size() == dimension);
    assert(scan.negative_off_diagonal_counts.size() == dimension);
    pivot_choice best{dimension, 3};
    for (size_t pivot = 0; pivot < dimension; ++pivot) {
        timeout_checkpoint();
        diagnostics::advance_preprocessing(pivot + 1, dimension);
        const size_t positive = scan.positive_off_diagonal_counts[pivot];
        const size_t negative = scan.negative_off_diagonal_counts[pivot];
        const size_t children = matrix(pivot, pivot).is_zero() || negative == 0 ? 1 : (positive == 0 || negative == 1 ? 2 : 3);
        if (children < best.children) {
            best = {pivot, children};
            if (children == 1) break;
        }
    }
    return best;
}

inline matrix_integer principal_block(const matrix_integer& matrix, const std::vector<size_t>& remaining)
{
    matrix_integer result(remaining.size(), remaining.size());
    for (size_t row = 0; row < remaining.size(); ++row) {
        timeout_checkpoint();
        for (size_t column = row; column < remaining.size(); ++column) {
            result(row, column) = matrix(remaining[row], remaining[column]);
            if (row != column) result(column, row) = result(row, column);
        }
    }
    return result;
}

inline matrix_integer schur_block(const matrix_integer& matrix, size_t pivot, const std::vector<size_t>& remaining,
                                  const std::vector<integer>& pivot_row)
{
    matrix_integer result(remaining.size(), remaining.size());
    for (size_t row = 0; row < remaining.size(); ++row) {
        timeout_checkpoint();
        for (size_t column = row; column < remaining.size(); ++column) {
            result(row, column).set_product(matrix(pivot, pivot), matrix(remaining[row], remaining[column]));
            result(row, column).submul(pivot_row[row], pivot_row[column]);
            if (row != column) result(column, row) = result(row, column);
        }
    }
    return result;
}

inline sparse_ray coordinate_ray(size_t index)
{
    sparse_ray ray;
    ray.indices[0] = index;
    ray.coefficients[0].set_one();
    return ray;
}

inline sparse_ray pair_ray(const std::vector<integer>& pivot_row, size_t positive, size_t negative)
{
    sparse_ray ray;
    ray.count = 2;
    ray.indices = {positive, negative};
    ray.coefficients[0].set_abs(pivot_row[negative]);
    ray.coefficients[1] = pivot_row[positive];

    integer divisor;
    fmpz_gcd(divisor.native_handle(), ray.coefficients[0].native_handle(), ray.coefficients[1].native_handle());
    ray.coefficients[0].divide_exact(divisor);
    ray.coefficients[1].divide_exact(divisor);
    return ray;
}

inline matrix_integer transform(const matrix_integer& matrix, const std::vector<sparse_ray>& rays)
{
    matrix_integer result(rays.size(), rays.size());
    integer coefficient;
    for (size_t row = 0; row < rays.size(); ++row) {
        timeout_checkpoint();
        for (size_t column = row; column < rays.size(); ++column) {
            for (size_t left = 0; left < rays[row].count; ++left) {
                for (size_t right = 0; right < rays[column].count; ++right) {
                    coefficient.set_product(rays[row].coefficients[left], rays[column].coefficients[right]);
                    result(row, column).addmul(coefficient, matrix(rays[row].indices[left], rays[column].indices[right]));
                }
            }
            if (row != column) result(column, row) = result(row, column);
        }
    }
    return result;
}

inline outcome combine(outcome first, outcome second) noexcept
{
    if (first == outcome::rejected || second == outcome::rejected) return outcome::rejected;
    if (first == outcome::accepted && second == outcome::accepted) return outcome::accepted;
    return outcome::unresolved;
}

} // namespace detail

/* One minimum-child Xu-Yao projection. More than two children or any unresolved child leaves this gate unresolved. */
template<typename ChildPreprocessor>
outcome check(const matrix_integer& matrix, const matrix_scan_result& scan, model::copositivity_mode mode,
              ChildPreprocessor&& preprocess_child)
{
    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::copomatrix, matrix.rows(), 0, matrix.rows());
    const int required_sign = mode == model::copositivity_mode::strictly_copositive ? 1 : 0;
    for (size_t index = 0; index < matrix.rows(); ++index) {
        timeout_checkpoint();
        if (matrix(index, index).sign() < required_sign) return outcome::rejected;
    }
    if (matrix.rows() == 1) return outcome::accepted;

    const detail::pivot_choice pivot = detail::minimum_small_pivot(matrix, scan);
    if (pivot.children > 2) return outcome::unresolved;

    std::vector<size_t> remaining;
    remaining.reserve(matrix.rows() - 1);
    for (size_t index = 0; index < matrix.rows(); ++index) {
        if (index != pivot.index) remaining.push_back(index);
    }

    std::vector<integer> pivot_row(remaining.size());
    std::vector<size_t> positive;
    std::vector<size_t> zero;
    std::vector<size_t> negative;
    for (size_t local = 0; local < remaining.size(); ++local) {
        pivot_row[local] = matrix(pivot.index, remaining[local]);
        if (pivot_row[local].sign() > 0) positive.push_back(local);
        else if (pivot_row[local].sign() < 0) negative.push_back(local);
        else zero.push_back(local);
    }

    const outcome principal = preprocess_child(detail::principal_block(matrix, remaining));
    if (principal == outcome::rejected) return outcome::rejected;
    if (matrix(pivot.index, pivot.index).is_zero()) return negative.empty() ? principal : outcome::rejected;
    if (negative.empty()) return principal;

    matrix_integer schur = detail::schur_block(matrix, pivot.index, remaining, pivot_row);
    if (positive.empty()) return detail::combine(principal, preprocess_child(schur));

    assert(negative.size() == 1 && pivot.children == 2);
    std::vector<detail::sparse_ray> rays;
    rays.reserve(remaining.size());
    for (size_t index : zero) rays.push_back(detail::coordinate_ray(index));
    rays.push_back(detail::coordinate_ray(negative.front()));
    for (size_t index : positive) rays.push_back(detail::pair_ray(pivot_row, index, negative.front()));
    return detail::combine(principal, preprocess_child(detail::transform(schur, rays)));
}

} // namespace coposit::copomatrix_precheck
