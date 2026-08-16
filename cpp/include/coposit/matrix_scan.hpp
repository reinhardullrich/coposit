#pragma once

#include <coposit/matrix_integer.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace coposit {

struct matrix_scan_requirements {
    bool negative_part_row_sums = false;
    bool all_ones = false;
    bool negative_graph = false;
    bool nonpositive_graph = false;
    bool copositive_principal_pairs = false;
    bool strict_principal_pairs = false;
    bool frank_wolfe = false;
    bool off_diagonal_sign_counts = false;
    bool motzkin_straus_pattern = false;
};

struct matrix_scan_result {
    size_t dimension = 0;
    bool all_diagonals_nonnegative = true;
    bool all_diagonals_positive = true;
    bool has_negative_off_diagonal = false;
    bool all_principal_pairs_copositive = true;
    bool all_principal_pairs_strictly_copositive = true;
    bool is_motzkin_straus_pattern = false;
    integer all_ones_quadratic_value;
    integer maximum_absolute_entry;
    std::vector<int> diagonal_signs;
    std::vector<integer> negative_part_row_sums;
    std::vector<integer> full_row_sums;
    std::vector<support> negative_neighbors;
    std::vector<support> nonpositive_neighbors;
    std::vector<size_t> positive_off_diagonal_counts;
    std::vector<size_t> negative_off_diagonal_counts;
    integer motzkin_straus_nonedge;
    integer motzkin_straus_edge;
    bool motzkin_straus_has_edge = false;
};

namespace matrix_scan_detail {

inline matrix_scan_result initialize(size_t dimension, const matrix_scan_requirements& requirements)
{
    matrix_scan_result result;
    result.dimension = dimension;
    result.diagonal_signs.resize(dimension);
    if (requirements.negative_part_row_sums) result.negative_part_row_sums.resize(dimension);
    if (requirements.frank_wolfe) result.full_row_sums.resize(dimension);
    if (requirements.negative_graph) {
        result.negative_neighbors.reserve(dimension);
        for (size_t index = 0; index < dimension; ++index) result.negative_neighbors.emplace_back(dimension);
    }
    if (requirements.nonpositive_graph) {
        result.nonpositive_neighbors.reserve(dimension);
        for (size_t index = 0; index < dimension; ++index) result.nonpositive_neighbors.emplace_back(dimension);
    }
    if (requirements.off_diagonal_sign_counts) {
        result.positive_off_diagonal_counts.resize(dimension);
        result.negative_off_diagonal_counts.resize(dimension);
    }
    return result;
}

inline void observe_diagonal(matrix_scan_result& result, const matrix_scan_requirements& requirements, size_t index,
                             integer::const_reference diagonal)
{
    const int sign = diagonal.sign();
    result.diagonal_signs[index] = sign;
    result.all_diagonals_nonnegative &= sign >= 0;
    result.all_diagonals_positive &= sign > 0;
    if (requirements.negative_part_row_sums) result.negative_part_row_sums[index] = diagonal;
    if (requirements.all_ones) result.all_ones_quadratic_value += diagonal;
    if (requirements.frank_wolfe) {
        result.full_row_sums[index] = diagonal;
        if (diagonal.compare_abs(result.maximum_absolute_entry) > 0) result.maximum_absolute_entry.set_abs(diagonal);
    }
    if (requirements.motzkin_straus_pattern) {
        if (index == 0) {
            result.motzkin_straus_nonedge = diagonal;
            result.is_motzkin_straus_pattern = sign >= 0;
        } else {
            result.is_motzkin_straus_pattern &= diagonal.compare(result.motzkin_straus_nonedge) == 0;
        }
    }
}

inline void observe_off_diagonal(matrix_scan_result& result, const matrix_scan_requirements& requirements, size_t row,
                                 size_t column, integer::const_reference entry, integer::const_reference row_diagonal,
                                 integer::const_reference column_diagonal)
{
    if (entry.sign() < 0) {
        result.has_negative_off_diagonal = true;
        if (requirements.off_diagonal_sign_counts) {
            ++result.negative_off_diagonal_counts[row];
            ++result.negative_off_diagonal_counts[column];
        }
        if (requirements.negative_part_row_sums) {
            result.negative_part_row_sums[row] += entry;
            result.negative_part_row_sums[column] += entry;
        }
        if (requirements.negative_graph) {
            result.negative_neighbors[row].set(column);
            result.negative_neighbors[column].set(row);
        }
        if (requirements.copositive_principal_pairs) {
            result.all_principal_pairs_copositive &= small_copositivity::check_2x2<model::copositivity_mode::copositive>(
                row_diagonal, entry, column_diagonal);
        }
        if (requirements.strict_principal_pairs) {
            result.all_principal_pairs_strictly_copositive &=
                small_copositivity::check_2x2<model::copositivity_mode::strictly_copositive>(
                    row_diagonal, entry, column_diagonal);
        }
    } else if (entry.sign() > 0 && requirements.off_diagonal_sign_counts) {
        ++result.positive_off_diagonal_counts[row];
        ++result.positive_off_diagonal_counts[column];
    }
    if (entry.sign() <= 0 && requirements.nonpositive_graph) {
        result.nonpositive_neighbors[row].set(column);
        result.nonpositive_neighbors[column].set(row);
    }
    if (requirements.all_ones) {
        result.all_ones_quadratic_value += entry;
        result.all_ones_quadratic_value += entry;
    }
    if (requirements.frank_wolfe) {
        result.full_row_sums[row] += entry;
        result.full_row_sums[column] += entry;
        if (entry.compare_abs(result.maximum_absolute_entry) > 0) result.maximum_absolute_entry.set_abs(entry);
    }
    if (requirements.motzkin_straus_pattern && result.is_motzkin_straus_pattern
        && entry.compare(result.motzkin_straus_nonedge) != 0) {
        if (entry.sign() >= 0) {
            result.is_motzkin_straus_pattern = false;
        } else if (!result.motzkin_straus_has_edge) {
            result.motzkin_straus_edge = entry;
            result.motzkin_straus_has_edge = true;
        } else {
            result.is_motzkin_straus_pattern &= entry.compare(result.motzkin_straus_edge) == 0;
        }
    }
}

inline void finalize(matrix_scan_result& result, const matrix_scan_requirements& requirements) noexcept
{
    if (requirements.motzkin_straus_pattern) result.is_motzkin_straus_pattern &= result.motzkin_straus_has_edge;
}

} // namespace matrix_scan_detail

inline matrix_scan_result scan_matrix(const matrix_integer& matrix, const matrix_scan_requirements& requirements)
{
    const size_t dimension = matrix.rows();
    matrix_scan_result result = matrix_scan_detail::initialize(dimension, requirements);
    for (size_t index = 0; index < dimension; ++index) {
        timeout_checkpoint();
        matrix_scan_detail::observe_diagonal(result, requirements, index, matrix(index, index));
    }

    for (size_t row = 0; row < dimension; ++row) {
        timeout_checkpoint();
        diagnostics::advance_preprocessing(row + 1, dimension);
        for (size_t column = row + 1; column < dimension; ++column) {
            matrix_scan_detail::observe_off_diagonal(result, requirements, row, column, matrix(row, column), matrix(row, row),
                                                      matrix(column, column));
        }
    }
    matrix_scan_detail::finalize(result, requirements);
    return result;
}

struct scanned_principal_matrix {
    matrix_integer matrix;
    matrix_scan_result scan;
};

inline scanned_principal_matrix scan_principal_matrix(const matrix_integer& source, const std::vector<size_t>& indices,
                                                      const matrix_scan_requirements& requirements)
{
    const size_t dimension = indices.size();
    scanned_principal_matrix result{matrix_integer(dimension, dimension),
                                    matrix_scan_detail::initialize(dimension, requirements)};

    for (size_t row = 0; row < dimension; ++row) {
        timeout_checkpoint();
        diagnostics::advance_preprocessing(row + 1, dimension);
        const size_t source_row = indices[row];
        result.matrix(row, row) = source(source_row, source_row);
        matrix_scan_detail::observe_diagonal(result.scan, requirements, row, result.matrix(row, row));
        for (size_t column = row + 1; column < dimension; ++column) {
            const size_t source_column = indices[column];
            result.matrix(row, column) = source(source_row, source_column);
            result.matrix(column, row) = result.matrix(row, column);
            matrix_scan_detail::observe_off_diagonal(result.scan, requirements, row, column, result.matrix(row, column),
                                                      result.matrix(row, row), source(source_column, source_column));
        }
    }
    matrix_scan_detail::finalize(result.scan, requirements);
    return result;
}

} // namespace coposit
