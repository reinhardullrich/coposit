#pragma once

#include <coposit/copomatrix_precheck.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace coposit::danninger_precheck {

using outcome = copomatrix_precheck::outcome;

namespace detail {

using pivot_choice = copomatrix_precheck::detail::pivot_choice;

inline pivot_choice minimum_small_pivot(const matrix_integer& matrix, const matrix_scan_result& scan,
                                       model::copositivity_mode mode)
{
    const size_t dimension = matrix.rows();
    assert(scan.dimension == dimension);
    assert(scan.positive_off_diagonal_counts.size() == dimension);
    assert(scan.negative_off_diagonal_counts.size() == dimension);
    const bool zero_diagonal = mode == model::copositivity_mode::copositive && !scan.all_diagonals_positive;
    pivot_choice best{dimension, 3};
    for (size_t pivot = 0; pivot < dimension; ++pivot) {
        timeout_checkpoint();
        diagnostics::advance_preprocessing(pivot + 1, dimension);
        const size_t positive = scan.positive_off_diagonal_counts[pivot];
        const size_t negative = scan.negative_off_diagonal_counts[pivot];
        size_t children;
        if (mode == model::copositivity_mode::copositive && matrix(pivot, pivot).is_zero()) children = negative == 0 ? 1 : 0;
        else children = positive == 0 || negative == 0 ? 1 : (positive == 1 && negative == 1 ? 2 : 3);
        if (children < best.children) {
            best = {pivot, children};
            if (children == 0 || (children == 1 && !zero_diagonal)) break;
        }
    }
    return best;
}

} // namespace detail

/* One minimum-child Danninger reduction. More than two children or any unresolved child leaves this gate unresolved. */
template<typename ChildPreprocessor>
outcome check(const matrix_integer& matrix, const matrix_scan_result& scan, model::copositivity_mode mode,
              ChildPreprocessor&& preprocess_child)
{
    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::danninger, matrix.rows(), 0, matrix.rows());
    const int required_sign = mode == model::copositivity_mode::strictly_copositive ? 1 : 0;
    for (size_t index = 0; index < matrix.rows(); ++index) {
        timeout_checkpoint();
        if (matrix(index, index).sign() < required_sign) return outcome::rejected;
    }
    if (matrix.rows() == 1) return outcome::accepted;

    const detail::pivot_choice pivot = detail::minimum_small_pivot(matrix, scan, mode);
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

    matrix_integer block = copomatrix_precheck::detail::principal_block(matrix, remaining);
    if (matrix(pivot.index, pivot.index).is_zero()) {
        return negative.empty() ? preprocess_child(block) : outcome::rejected;
    }
    if (negative.empty()) return preprocess_child(block);

    matrix_integer schur = copomatrix_precheck::detail::schur_block(matrix, pivot.index, remaining, pivot_row);
    if (positive.empty()) return preprocess_child(schur);

    assert(positive.size() == 1 && negative.size() == 1 && pivot.children == 2);
    const copomatrix_precheck::detail::sparse_ray boundary =
        copomatrix_precheck::detail::pair_ray(pivot_row, positive.front(), negative.front());
    std::vector<copomatrix_precheck::detail::sparse_ray> rays;
    rays.reserve(remaining.size());
    for (size_t index : zero) rays.push_back(copomatrix_precheck::detail::coordinate_ray(index));
    rays.push_back(copomatrix_precheck::detail::coordinate_ray(positive.front()));
    rays.push_back(boundary);
    const outcome plus = preprocess_child(copomatrix_precheck::detail::transform(block, rays));
    if (plus == outcome::rejected) return outcome::rejected;

    rays.resize(zero.size());
    rays.push_back(copomatrix_precheck::detail::coordinate_ray(negative.front()));
    rays.push_back(boundary);
    return copomatrix_precheck::detail::combine(plus, preprocess_child(copomatrix_precheck::detail::transform(schur, rays)));
}

} // namespace coposit::danninger_precheck
