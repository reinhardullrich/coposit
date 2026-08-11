#pragma once

/*
 * The immediate-integer Bareiss update below is adapted from FLINT's fmpz_mat_fflu:
 *
 *     Copyright (C) 2011 Fredrik Johansson
 *
 * FLINT is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public
 * License (LGPL) as published by the Free Software Foundation; either version 3 of the License, or (at your option) any
 * later version.
 * See <https://www.gnu.org/licenses/>.
 */

#include <flint/fmpz_mat.h>
#include <flint/ulong_extras.h>

#include <cassert>
#include <cstddef>
#include <vector>

#include <coposit/matrix_integer.hpp>
#include <coposit/timeout.hpp>

namespace coposit {

/*
 * Reusable exact factorization of a symmetric integer matrix. factorize_inplace() overwrites the lower triangle of the
 * supplied matrix. The caller keeps that matrix and can then solve any number of right-hand sides without refactoring.
 * One object must not serve concurrent solves because it owns reusable arithmetic scratch.
 */
class fraction_free_ldlt_factorization {
public:
    explicit fraction_free_ldlt_factorization(size_t maximum_dimension)
        : coordinate_operations_(2 * maximum_dimension)
    {
        fmpz_init(determinant_);
        fmpz_init(previous_pivot_);
        fmpz_init(temporary_1_);
        fmpz_init(temporary_2_);
    }

    fraction_free_ldlt_factorization(const fraction_free_ldlt_factorization&) = delete;
    fraction_free_ldlt_factorization& operator=(const fraction_free_ldlt_factorization&) = delete;

    ~fraction_free_ldlt_factorization()
    {
        fmpz_clear(temporary_2_);
        fmpz_clear(temporary_1_);
        fmpz_clear(previous_pivot_);
        fmpz_clear(determinant_);
    }

    /*
     * Factor a symmetric integer matrix in place. Returns 1 for a nonsingular matrix and 0 for a singular matrix. The
     * exact signed determinant and rank are available after either result; a singular factorization must not be passed to
     * solve_inplace().
     */
    int factorize_inplace(matrix_integer& system)
    {
        assert(system.rows() == system.cols());

        fmpz_mat_struct* const raw_system = system.native_handle();
        const size_t dimension = system.rows();

        dimension_ = dimension;
        rank_ = 0;
        operation_count_ = 0;
        nonsingular_ = false;
        factorization_is_immediate_ = true;
        positive_inertia_ = 0;
        fmpz_zero(determinant_);
        assert(coordinate_operations_.size() >= 2 * dimension);

        if (dimension == 0) {
            fmpz_one(determinant_);
            nonsingular_ = true;
            return 1;
        }

        bool immediate = all_values_are_immediate(raw_system, dimension);
        bool previous_is_one = true;
        ulong unsigned_denominator = 0;
        ulong denominator_inverse = 0;
        slong normalization_shift = 0;
        bool denominator_is_negative = false;
        int previous_pivot_sign = 1;

        for (size_t pivot_position = 0; pivot_position < dimension; ++pivot_position) {
            timeout_checkpoint();
            // Failure means that every diagonal and off-diagonal entry in the active symmetric block is zero. The completed
            // pivots therefore give the exact rank, not merely a lower bound.
            if (!select_nonzero_diagonal(raw_system, pivot_position, dimension, immediate)) return 0;
            rank_ = pivot_position + 1;
            fmpz* const pivot = entry(raw_system, pivot_position, pivot_position);

            // p_k/p_(k-1) is the corresponding ordinary LDL^T diagonal entry, so its sign contributes one inertia
            // count.
            const int diagonal_sign = fmpz_sgn(pivot) * previous_pivot_sign;
            if (diagonal_sign > 0) ++positive_inertia_;
            previous_pivot_sign = fmpz_sgn(pivot);

            if (pivot_position + 1 == dimension) break;

            bool next_step_is_immediate = immediate;
            if (immediate) {
                // Every source is immediate at this step's start. A large destination only selects the arbitrary-precision
                // path for the next step, so the complete current triangle can use FLINT's two-limb update.
                for (size_t row = pivot_position + 1; row < dimension; ++row) {
                    for (size_t column = pivot_position + 1; column <= row; ++column) {
                        next_step_is_immediate &= update_immediate(
                            entry(raw_system, row, column), pivot, entry(raw_system, row, pivot_position),
                            entry(raw_system, column, pivot_position), pivot_position > 0, previous_is_one,
                            previous_pivot_, unsigned_denominator, denominator_inverse, normalization_shift,
                            denominator_is_negative);
                    }
                }
            } else {
                for (size_t row = pivot_position + 1; row < dimension; ++row) {
                    for (size_t column = pivot_position + 1; column <= row; ++column) {
                        fmpz* const result = entry(raw_system, row, column);
                        fmpz_mul(result, result, pivot);
                        fmpz_submul(result, entry(raw_system, row, pivot_position),
                                    entry(raw_system, column, pivot_position));
                        if (pivot_position > 0 && !previous_is_one) fmpz_divexact(result, result, previous_pivot_);
                    }
                }
            }

            fmpz_set(previous_pivot_, pivot);
            previous_is_one = fmpz_is_one(previous_pivot_);
            immediate = next_step_is_immediate;

            if (immediate) {
                unsigned_denominator = FLINT_ABS(static_cast<slong>(*previous_pivot_));
                denominator_is_negative = static_cast<slong>(*previous_pivot_) < 0;
                normalization_shift = flint_clz(unsigned_denominator);
                denominator_inverse = n_preinvert_limb_prenorm(unsigned_denominator << normalization_shift);
            }
        }

        // Symmetric swaps and additions are unimodular congruences, so the final Bareiss pivot is the input
        // determinant.
        fmpz_set(determinant_, entry(raw_system, dimension - 1, dimension - 1));
        factorization_is_immediate_ = immediate;
        nonsingular_ = true;
        return 1;
    }

    integer::const_reference determinant() const noexcept { return integer::const_reference(determinant_); }
    size_t rank() const noexcept { return rank_; }
    bool is_positive_definite() const noexcept { return nonsingular_ && positive_inertia_ == dimension_; }
    bool is_positive_semidefinite() const noexcept { return positive_inertia_ == rank_; }

    /*
     * Recover one nonzero exact vector from the nullspace of a retained singular factorization. Only one free transformed
     * coordinate is used, regardless of nullity; no nullspace basis is constructed.
     */
    void one_nullspace_vector(matrix_integer& result, const matrix_integer& factored_system) const
    {
        assert(!nonsingular_);
        assert(rank_ < dimension_);
        assert(factored_system.rows() == dimension_);
        assert(factored_system.cols() == dimension_);
        assert(result.rows() == dimension_);
        assert(result.cols() == 1);

        fmpz_mat_struct* const raw_result = result.native_handle();
        const fmpz_mat_struct* const raw_system = factored_system.native_handle();
        fmpz_mat_zero(raw_result);

        if (rank_ == 0) {
            fmpz_one(rhs_entry(raw_result, 0, 0));
        } else {
            // The last pivot is the determinant of the nonsingular leading block. Using it as the free coordinate makes every
            // triangular division exact (Cramer's rule) without introducing rational storage.
            fmpz_abs(rhs_entry(raw_result, rank_, 0), entry(raw_system, rank_ - 1, rank_ - 1));
            for (size_t row = rank_; row-- > 0; ) {
                timeout_checkpoint();
                fmpz* const value = rhs_entry(raw_result, row, 0);
                for (size_t solved_row = row + 1; solved_row <= rank_; ++solved_row) {
                    fmpz_submul(value, entry(raw_system, solved_row, row), rhs_entry(raw_result, solved_row, 0));
                }
                fmpz_divexact(value, value, entry(raw_system, row, row));
            }
        }

        restore_original_coordinates(raw_result, 1);
    }

    /*
     * Recover an exact basis of the nullspace from a retained singular factorization. Each column fixes one free transformed
     * coordinate, solves the pivot equations backwards, and is then restored to the input coordinate system.
     */
    void nullspace_basis(matrix_integer& result, const matrix_integer& factored_system) const
    {
        assert(!nonsingular_);
        assert(rank_ < dimension_);
        assert(factored_system.rows() == dimension_);
        assert(factored_system.cols() == dimension_);
        assert(result.rows() == dimension_);
        assert(result.cols() == dimension_ - rank_);

        fmpz_mat_struct* const raw_result = result.native_handle();
        const fmpz_mat_struct* const raw_system = factored_system.native_handle();
        const size_t nullity = dimension_ - rank_;
        fmpz_mat_zero(raw_result);

        if (rank_ == 0) {
            for (size_t column = 0; column < nullity; ++column) fmpz_one(rhs_entry(raw_result, column, column));
        } else {
            for (size_t column = 0; column < nullity; ++column) {
                const size_t free_coordinate = rank_ + column;
                fmpz_abs(rhs_entry(raw_result, free_coordinate, column), entry(raw_system, rank_ - 1, rank_ - 1));
                for (size_t row = rank_; row-- > 0; ) {
                    timeout_checkpoint();
                    fmpz* const value = rhs_entry(raw_result, row, column);
                    for (size_t solved_row = row + 1; solved_row < dimension_; ++solved_row) {
                        fmpz_submul(value, entry(raw_system, solved_row, row), rhs_entry(raw_result, solved_row, column));
                    }
                    fmpz_divexact(value, value, entry(raw_system, row, row));
                }
            }
        }

        restore_original_coordinates(raw_result, nullity);
    }

    /*
     * Solve system*X = right_hand_sides*denominator from a retained nonsingular factorization. right_hand_sides is
     * overwritten by the integer numerator matrix X; denominator is positive. Columns are independent right-hand sides.
     */
    void solve_inplace(matrix_integer& right_hand_sides, integer& denominator, const matrix_integer& factored_system) const
    {
        assert(nonsingular_);
        assert(factored_system.rows() == dimension_);
        assert(factored_system.cols() == dimension_);
        assert(right_hand_sides.rows() == dimension_);

        fmpz_mat_struct* const raw_right_hand_sides = right_hand_sides.native_handle();
        const fmpz_mat_struct* const raw_system = factored_system.native_handle();
        const size_t right_hand_side_count = right_hand_sides.cols();

        fmpz_abs(denominator.native_handle(), determinant_);
        if (dimension_ == 0) return;

        bool immediate = factorization_is_immediate_ &&
                         all_values_are_immediate(raw_right_hand_sides, dimension_, right_hand_side_count);
        bool previous_is_one = true;
        ulong unsigned_denominator = 0;
        ulong denominator_inverse = 0;
        slong normalization_shift = 0;
        bool denominator_is_negative = false;

        // Later congruences also transform completed factor columns. The retained triangle therefore uses the final
        // coordinate system, so transform every new right-hand side completely before replaying Bareiss elimination.
        for (size_t index = 0; index < operation_count_; ++index) {
            apply_operation_to_right_hand_sides(raw_right_hand_sides, right_hand_side_count,
                                                coordinate_operations_[index], immediate);
        }

        for (size_t pivot_position = 0; pivot_position < dimension_; ++pivot_position) {
            timeout_checkpoint();
            if (pivot_position + 1 == dimension_) break;

            const fmpz* const pivot = entry(raw_system, pivot_position, pivot_position);
            bool next_step_is_immediate = immediate;
            if (immediate) {
                for (size_t row = pivot_position + 1; row < dimension_; ++row) {
                    for (size_t column = 0; column < right_hand_side_count; ++column) {
                        next_step_is_immediate &= update_immediate(
                            rhs_entry(raw_right_hand_sides, row, column), pivot, entry(raw_system, row, pivot_position),
                            rhs_entry(raw_right_hand_sides, pivot_position, column), pivot_position > 0,
                            previous_is_one, previous_pivot_, unsigned_denominator, denominator_inverse,
                            normalization_shift, denominator_is_negative);
                    }
                }
            } else {
                for (size_t row = pivot_position + 1; row < dimension_; ++row) {
                    for (size_t column = 0; column < right_hand_side_count; ++column) {
                        fmpz* const result = rhs_entry(raw_right_hand_sides, row, column);
                        fmpz_mul(result, result, pivot);
                        fmpz_submul(result, entry(raw_system, row, pivot_position),
                                    rhs_entry(raw_right_hand_sides, pivot_position, column));
                        if (pivot_position > 0 && !previous_is_one) fmpz_divexact(result, result, previous_pivot_);
                    }
                }
            }
            fmpz_set(previous_pivot_, pivot);
            previous_is_one = fmpz_is_one(previous_pivot_);
            immediate = next_step_is_immediate;

            if (immediate) {
                unsigned_denominator = FLINT_ABS(static_cast<slong>(*previous_pivot_));
                denominator_is_negative = static_cast<slong>(*previous_pivot_) < 0;
                normalization_shift = flint_clz(unsigned_denominator);
                denominator_inverse = n_preinvert_limb_prenorm(unsigned_denominator << normalization_shift);
            }
        }
        for (size_t column = 0; column < right_hand_side_count; ++column) {
            timeout_checkpoint();
            for (size_t row = dimension_; row-- > 0; ) {
                fmpz* const numerator = rhs_entry(raw_right_hand_sides, row, column);
                fmpz_mul(numerator, denominator.native_handle(), numerator);
                for (size_t solved_row = row + 1; solved_row < dimension_; ++solved_row) {
                    fmpz_submul(numerator, entry(raw_system, solved_row, row),
                                rhs_entry(raw_right_hand_sides, solved_row, column));
                }
                fmpz_divexact(numerator, numerator, entry(raw_system, row, row));
            }
        }

        restore_original_coordinates(raw_right_hand_sides, right_hand_side_count);
    }

private:
    enum class coordinate_operation_kind : unsigned char { swap, add };

    struct coordinate_operation {
        coordinate_operation_kind kind;
        size_t target;
        size_t source;
    };

    static ulong right_shift(ulong value, unsigned int count) noexcept
    {
        return count == FLINT_BITS ? UWORD(0) : value >> count;
    }

    static fmpz* entry(fmpz_mat_struct* system, size_t row, size_t column) noexcept
    {
        return fmpz_mat_entry(system, static_cast<slong>(row), static_cast<slong>(column));
    }

    static const fmpz* entry(const fmpz_mat_struct* system, size_t row, size_t column) noexcept
    {
        return fmpz_mat_entry(system, static_cast<slong>(row), static_cast<slong>(column));
    }

    static fmpz* lower_entry(fmpz_mat_struct* system, size_t first, size_t second) noexcept
    {
        return first >= second ? entry(system, first, second) : entry(system, second, first);
    }

    static fmpz* rhs_entry(fmpz_mat_struct* right_hand_sides, size_t row, size_t column) noexcept
    {
        return fmpz_mat_entry(right_hand_sides, static_cast<slong>(row), static_cast<slong>(column));
    }

    static const fmpz* rhs_entry(const fmpz_mat_struct* right_hand_sides, size_t row, size_t column) noexcept
    {
        return fmpz_mat_entry(right_hand_sides, static_cast<slong>(row), static_cast<slong>(column));
    }

    void remember_operation(coordinate_operation_kind kind, size_t target, size_t source)
    {
        assert(operation_count_ < coordinate_operations_.size());
        coordinate_operations_[operation_count_++] = {kind, target, source};
    }

    void swap_symmetric_coordinates(fmpz_mat_struct* system, size_t completed, size_t dimension, size_t first,
                                    size_t second)
    {
        if (first == second) return;

        for (size_t column = 0; column < completed; ++column) {
            fmpz_swap(entry(system, first, column), entry(system, second, column));
        }
        fmpz_swap(entry(system, first, first), entry(system, second, second));

        for (size_t index = completed; index < dimension; ++index) {
            if (index == first || index == second) continue;
            fmpz_swap(lower_entry(system, first, index), lower_entry(system, second, index));
        }

        remember_operation(coordinate_operation_kind::swap, first, second);
    }

    void add_symmetric_coordinate(fmpz_mat_struct* system, size_t completed, size_t dimension, size_t target,
                                  size_t source)
    {
        for (size_t column = 0; column < completed; ++column) {
            fmpz_add(entry(system, target, column), entry(system, target, column), entry(system, source, column));
        }

        fmpz_set(temporary_1_, entry(system, target, target));
        fmpz_set(temporary_2_, lower_entry(system, target, source));

        for (size_t index = completed; index < dimension; ++index) {
            if (index == target || index == source) continue;
            fmpz_add(lower_entry(system, target, index), lower_entry(system, target, index),
                     lower_entry(system, source, index));
        }

        fmpz_add(lower_entry(system, target, source), temporary_2_, entry(system, source, source));
        fmpz_add(entry(system, target, target), temporary_1_, temporary_2_);
        fmpz_add(entry(system, target, target), entry(system, target, target), temporary_2_);
        fmpz_add(entry(system, target, target), entry(system, target, target), entry(system, source, source));

        remember_operation(coordinate_operation_kind::add, target, source);
    }

    static bool all_values_are_immediate(const fmpz_mat_struct* system, size_t dimension) noexcept
    {
        for (size_t row = 0; row < dimension; ++row) {
            for (size_t column = 0; column <= row; ++column) {
                if (COEFF_IS_MPZ(*entry(system, row, column))) return false;
            }
        }
        return true;
    }

    static bool all_values_are_immediate(const fmpz_mat_struct* right_hand_sides, size_t dimension,
                                         size_t right_hand_side_count) noexcept
    {
        for (size_t row = 0; row < dimension; ++row) {
            for (size_t column = 0; column < right_hand_side_count; ++column) {
                if (COEFF_IS_MPZ(*rhs_entry(right_hand_sides, row, column))) return false;
            }
        }
        return true;
    }

    /* Compute result=(result*pivot-left*right)/previous_pivot while every input is a FLINT immediate integer. */
    static bool update_immediate(fmpz* result, const fmpz* pivot, const fmpz* left, const fmpz* right,
                                 bool divide_by_previous, bool previous_is_one, const fmpz* previous_pivot,
                                 ulong unsigned_denominator, ulong denominator_inverse, slong normalization_shift,
                                 bool denominator_is_negative)
    {
        ulong first_high, first_low, second_high, second_low;
        smul_ppmm(first_high, first_low, *result, *pivot);
        smul_ppmm(second_high, second_low, *left, *right);
        sub_ddmmss(first_high, first_low, first_high, first_low, second_high, second_low);

        const bool result_is_negative = static_cast<slong>(first_high) < 0;
        if (result_is_negative) sub_ddmmss(first_high, first_low, UWORD(0), UWORD(0), first_high, first_low);

        if (divide_by_previous && !previous_is_one) {
            if (first_high >= unsigned_denominator) {
                fmpz_set_uiui(result, first_high, first_low);
                if (result_is_negative) fmpz_neg(result, result);
                fmpz_divexact(result, result, previous_pivot);
            } else {
                ulong quotient;
                ulong FLINT_SET_BUT_UNUSED(remainder);
                udiv_qrnnd_preinv(quotient, remainder,
                                  (first_high << normalization_shift) +
                                      right_shift(first_low, FLINT_BITS - normalization_shift),
                                  first_low << normalization_shift, unsigned_denominator << normalization_shift,
                                  denominator_inverse);
                if (result_is_negative != denominator_is_negative) fmpz_neg_ui(result, quotient);
                else fmpz_set_ui(result, quotient);
            }
        } else {
            if (first_high > 0) fmpz_set_uiui(result, first_high, first_low);
            else fmpz_set_ui(result, first_low);
            if (result_is_negative) fmpz_neg(result, result);
        }

        return !COEFF_IS_MPZ(*result);
    }

    bool select_nonzero_diagonal(fmpz_mat_struct* system, size_t pivot_position, size_t dimension, bool& immediate)
    {
        size_t diagonal_pivot = pivot_position;
        while (diagonal_pivot < dimension && fmpz_is_zero(entry(system, diagonal_pivot, diagonal_pivot))) {
            ++diagonal_pivot;
        }

        if (diagonal_pivot < dimension) {
            swap_symmetric_coordinates(system, pivot_position, dimension, pivot_position, diagonal_pivot);
            return true;
        }

        size_t first = dimension;
        size_t second = dimension;
        for (size_t row = pivot_position + 1; row < dimension && first == dimension; ++row) {
            for (size_t column = pivot_position; column < row; ++column) {
                if (!fmpz_is_zero(entry(system, row, column))) {
                    first = column;
                    second = row;
                    break;
                }
            }
        }
        if (first == dimension) return false;

        swap_symmetric_coordinates(system, pivot_position, dimension, pivot_position, first);
        add_symmetric_coordinate(system, pivot_position, dimension, pivot_position, second);
        if (immediate) immediate = all_values_are_immediate(system, dimension);
        assert(!fmpz_is_zero(entry(system, pivot_position, pivot_position)));
        return true;
    }

    static void apply_operation_to_right_hand_sides(fmpz_mat_struct* right_hand_sides, size_t right_hand_side_count,
                                                    const coordinate_operation& operation, bool& immediate)
    {
        for (size_t column = 0; column < right_hand_side_count; ++column) {
            if (operation.kind == coordinate_operation_kind::swap) {
                fmpz_swap(rhs_entry(right_hand_sides, operation.target, column),
                          rhs_entry(right_hand_sides, operation.source, column));
            } else {
                fmpz* const target = rhs_entry(right_hand_sides, operation.target, column);
                fmpz_add(target, target, rhs_entry(right_hand_sides, operation.source, column));
                if (immediate && COEFF_IS_MPZ(*target)) immediate = false;
            }
        }
    }

    void restore_original_coordinates(fmpz_mat_struct* solution, size_t right_hand_side_count) const
    {
        for (size_t index = operation_count_; index-- > 0; ) {
            const coordinate_operation operation = coordinate_operations_[index];
            for (size_t column = 0; column < right_hand_side_count; ++column) {
                if (operation.kind == coordinate_operation_kind::swap) {
                    fmpz_swap(rhs_entry(solution, operation.target, column),
                              rhs_entry(solution, operation.source, column));
                } else {
                    // The forward congruence used old=T*new with T=I+e_source*e_target^T,
                    // hence x_source <- x_source+x_target.
                    fmpz_add(rhs_entry(solution, operation.source, column),
                             rhs_entry(solution, operation.source, column),
                             rhs_entry(solution, operation.target, column));
                }
            }
        }
    }

    size_t dimension_ = 0;
    size_t rank_ = 0;
    size_t operation_count_ = 0;
    bool nonsingular_ = false;
    bool factorization_is_immediate_ = true;
    size_t positive_inertia_ = 0;
    std::vector<coordinate_operation> coordinate_operations_;
    fmpz_t determinant_;
    mutable fmpz_t previous_pivot_;
    fmpz_t temporary_1_;
    fmpz_t temporary_2_;
};

} // namespace coposit
