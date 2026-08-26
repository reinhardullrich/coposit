#include <coposit/heuristic_kkt_search.hpp>

#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/matrix_integer.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::pre_check::detail {

// One bounded active-set walk from the centre of the simplex. Floating arithmetic only chooses the path; every reported sign is exact.
class heuristic_kkt_search {
public:
    struct result {
        std::optional<int> sign;
        size_t visited = 0;
        bool exact_continuation = false;
        bool reached_kkt = false;
    };

    explicit heuristic_kkt_search(size_t dimension)
        : support_context_(dimension)
        , dimension_(dimension)
        , candidate_support_(support_context_.make())
        , visited_(support_less{&support_context_})
        , factorization_(dimension > 0 ? dimension - 1 : 0)
    {
    }

    result run(const matrix_integer& matrix)
    {
        if (matrix.rows() != dimension_) throw std::logic_error("heuristic KKT search dimension mismatch");
        prepare_floating_matrix(matrix);
        indices_.resize(dimension_);
        std::iota(indices_.begin(), indices_.end(), size_t{0});
        visited_.clear();
        write_support(indices_, candidate_support_);
        visited_.insert(support_context_.clone(candidate_support_));

        result outcome;
        bool exact_mode = false;
        while (outcome.visited < dimension_) {
            timeout_checkpoint();
            ++outcome.visited;
            diagnostics::advance_preprocessing(outcome.visited, dimension_);
            if (exact_mode) {
                const exact_face face = analyze_exact(matrix);
                if (face.consistent && face.feasible && payoff_.sign() < 0) {
                    outcome.sign = -1;
                    outcome.reached_kkt = face.is_kkt;
                    return outcome;
                }
                if (face.consistent && face.feasible && face.is_kkt) {
                    outcome.sign = payoff_.sign();
                    outcome.reached_kkt = true;
                    return outcome;
                }
                auto next = face.nonsingular ? exact_nonsingular_successor(face) : exact_singular_successor(face);
                if (!next) return outcome;
                move_to(std::move(*next));
                continue;
            }

            const floating_face face = analyze_floating();
            if (face.inconclusive) return outcome;
            std::optional<exact_face> exact_candidate;
            if (face.negative_witness_candidate) {
                exact_candidate = analyze_exact(matrix);
                if (exact_candidate->consistent && exact_candidate->feasible && payoff_.sign() < 0) {
                    outcome.sign = -1;
                    outcome.reached_kkt = exact_candidate->is_kkt;
                    return outcome;
                }
            }
            if (face.terminal_candidate) {
                const exact_face exact = exact_candidate ? *exact_candidate : analyze_exact(matrix);
                if (exact.consistent && exact.feasible && exact.is_kkt) {
                    outcome.sign = payoff_.sign();
                    outcome.reached_kkt = true;
                    return outcome;
                }
                exact_mode = true;
                outcome.exact_continuation = true;
                if (exact.consistent && exact.feasible && payoff_.sign() < 0) {
                    outcome.sign = -1;
                    return outcome;
                }
                auto next = exact.nonsingular ? exact_nonsingular_successor(exact) : exact_singular_successor(exact);
                if (!next) return outcome;
                move_to(std::move(*next));
                continue;
            }

            auto next = floating_successor(face);
            if (!next) return outcome;
            move_to(std::move(*next));
        }
        return outcome;
    }

private:
    class double_matrix {
    public:
        void resize(size_t rows, size_t columns)
        {
            columns_ = columns;
            values_.assign(rows * columns, 0.0);
        }
        double& operator()(size_t row, size_t column) noexcept { return values_[row * columns_ + column]; }
        double operator()(size_t row, size_t column) const noexcept { return values_[row * columns_ + column]; }

    private:
        size_t columns_ = 0;
        std::vector<double> values_;
    };

    struct floating_face {
        bool terminal_candidate = false;
        double tolerance = 0.0;
        bool inconclusive = false;
        bool negative_witness_candidate = false;
    };

    struct exact_face {
        bool nonsingular = true;
        bool consistent = true;
        bool feasible = false;
        bool is_kkt = false;
        size_t nullity = 0;
    };

    static constexpr double bunch_kaufman_alpha = 0.6403882032022076;
    static constexpr double floating_pivot_cutoff = 64.0 * std::numeric_limits<double>::epsilon();

    /*
     * Lower-triangle, one-RHS subset of LAPACK's DSYTF2/DSYTRS path, adapted from FracESSA's fast candidate filter.
     * Copyright (c) 1992-2023 The University of Tennessee and The University of Tennessee Research Foundation.
     * Copyright (c) 2000-2023 The University of California Berkeley.
     * Copyright (c) 2006-2023 The University of Colorado Denver.
     * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following
     * conditions are met:
     *
     * - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
     * - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
     *   disclaimer in the documentation and/or other materials provided with the distribution.
     * - Neither the name of the copyright holders nor the names of its contributors may be used to endorse or promote products
     *   derived from this software without specific prior written permission.
     *
     * The copyright holders provide no reassurances that the source code provided does not infringe any patent, copyright, or other
     * intellectual property rights of third parties. The copyright holders disclaim liability to any recipient for claims brought
     * by any third party for infringement of that party's intellectual property rights.
     *
     * THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
     * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
     * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
     * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
     * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
     */
    static void swap_active_coordinates(
        double_matrix& system, size_t active_start, size_t first, size_t second, size_t dimension, size_t pivot_size)
    {
        if (first == second) return;
        for (size_t row = second + 1; row < dimension; ++row) std::swap(system(row, first), system(row, second));
        for (size_t row = first + 1; row < second; ++row) std::swap(system(row, first), system(second, row));
        std::swap(system(first, first), system(second, second));
        if (pivot_size == 2) std::swap(system(active_start + 1, active_start), system(second, active_start));
    }

    static bool factor_and_forward_solve(
        double_matrix& system, size_t dimension, std::vector<int>& pivots, std::vector<double>& solution)
    {
        size_t k = 0;
        while (k < dimension) {
            size_t pivot_size = 1;
            size_t pivot_index = k;
            const double diagonal_magnitude = std::abs(system(k, k));
            size_t column_max_index = k;
            double column_maximum = 0.0;
            for (size_t row = k + 1; row < dimension; ++row) {
                const double magnitude = std::abs(system(row, k));
                if (magnitude > column_maximum) {
                    column_maximum = magnitude;
                    column_max_index = row;
                }
            }
            const double active_maximum = std::max(diagonal_magnitude, column_maximum);
            if (!std::isfinite(active_maximum) || active_maximum < floating_pivot_cutoff) return false;
            if (diagonal_magnitude < bunch_kaufman_alpha * column_maximum) {
                double row_maximum = 0.0;
                for (size_t column = k; column < column_max_index; ++column)
                    row_maximum = std::max(row_maximum, std::abs(system(column_max_index, column)));
                for (size_t row = column_max_index + 1; row < dimension; ++row)
                    row_maximum = std::max(row_maximum, std::abs(system(row, column_max_index)));
                if (!std::isfinite(row_maximum) || row_maximum == 0.0) return false;
                if (diagonal_magnitude >= bunch_kaufman_alpha * column_maximum * (column_maximum / row_maximum)) {
                    pivot_index = k;
                } else if (std::abs(system(column_max_index, column_max_index)) >= bunch_kaufman_alpha * row_maximum) {
                    pivot_index = column_max_index;
                } else {
                    pivot_index = column_max_index;
                    pivot_size = 2;
                }
            }

            const size_t block_last = k + pivot_size - 1;
            swap_active_coordinates(system, k, block_last, pivot_index, dimension, pivot_size);
            if (pivot_index != block_last) std::swap(solution[block_last], solution[pivot_index]);
            if (pivot_size == 1) {
                const double pivot = system(k, k);
                if (!std::isfinite(pivot) || std::abs(pivot) < floating_pivot_cutoff) return false;
                const double inverse_pivot = 1.0 / pivot;
                for (size_t column = k + 1; column < dimension; ++column) {
                    const double multiplier = system(column, k) * inverse_pivot;
                    for (size_t row = column; row < dimension; ++row) system(row, column) -= system(row, k) * multiplier;
                }
                for (size_t row = k + 1; row < dimension; ++row) {
                    system(row, k) *= inverse_pivot;
                    solution[row] -= system(row, k) * solution[k];
                }
                solution[k] /= pivot;
                pivots[k] = static_cast<int>(pivot_index + 1);
            } else {
                const double off_diagonal = system(k + 1, k);
                if (!std::isfinite(off_diagonal) || std::abs(off_diagonal) < floating_pivot_cutoff) return false;
                const double lower_diagonal = system(k + 1, k + 1) / off_diagonal;
                const double upper_diagonal = system(k, k) / off_diagonal;
                const double determinant_factor = lower_diagonal * upper_diagonal - 1.0;
                if (!std::isfinite(determinant_factor) || std::abs(determinant_factor) < floating_pivot_cutoff) return false;
                const double inverse_block_scale = 1.0 / (determinant_factor * off_diagonal);
                for (size_t column = k + 2; column < dimension; ++column) {
                    const double first_multiplier =
                        inverse_block_scale * (lower_diagonal * system(column, k) - system(column, k + 1));
                    const double second_multiplier =
                        inverse_block_scale * (upper_diagonal * system(column, k + 1) - system(column, k));
                    for (size_t row = column; row < dimension; ++row)
                        system(row, column) -= system(row, k) * first_multiplier + system(row, k + 1) * second_multiplier;
                    system(column, k) = first_multiplier;
                    system(column, k + 1) = second_multiplier;
                    solution[column] -= first_multiplier * solution[k] + second_multiplier * solution[k + 1];
                }
                const double first_rhs = solution[k] / off_diagonal;
                const double second_rhs = solution[k + 1] / off_diagonal;
                solution[k] = (lower_diagonal * first_rhs - second_rhs) / determinant_factor;
                solution[k + 1] = (upper_diagonal * second_rhs - first_rhs) / determinant_factor;
                pivots[k] = pivots[k + 1] = -static_cast<int>(pivot_index + 1);
            }
            k += pivot_size;
        }
        return true;
    }

    static bool solve_backward(
        const double_matrix& system, size_t dimension, const std::vector<int>& pivots, std::vector<double>& solution)
    {
        size_t k = dimension;
        while (k > 0) {
            const size_t block_last = k - 1;
            if (pivots[block_last] > 0) {
                for (size_t row = block_last + 1; row < dimension; ++row)
                    solution[block_last] -= system(row, block_last) * solution[row];
                const size_t pivot_index = static_cast<size_t>(pivots[block_last] - 1);
                if (pivot_index != block_last) std::swap(solution[block_last], solution[pivot_index]);
                --k;
            } else {
                const size_t block_first = block_last - 1;
                for (size_t row = block_last + 1; row < dimension; ++row) {
                    solution[block_last] -= system(row, block_last) * solution[row];
                    solution[block_first] -= system(row, block_first) * solution[row];
                }
                const size_t pivot_index = static_cast<size_t>(-pivots[block_last] - 1);
                if (pivot_index != block_last) std::swap(solution[block_last], solution[pivot_index]);
                k -= 2;
            }
        }
        return std::all_of(solution.begin(), solution.end(), [](double value) { return std::isfinite(value); });
    }

    void prepare_floating_matrix(const matrix_integer& matrix)
    {
        floating_matrix_.resize(dimension_, dimension_);
        integer maximum;
        bool initialized = false;
        for (size_t row = 0; row < dimension_; ++row) {
            for (size_t column = 0; column <= row; ++column) {
                const auto value = matrix(row, column);
                if (value.is_zero()) continue;
                if (!initialized || value.compare_abs(maximum) > 0) {
                    maximum.set_abs(value);
                    initialized = true;
                }
            }
        }
        if (!initialized) return;
        slong maximum_exponent = 0;
        static_cast<void>(maximum.to_dbl_2exp(maximum_exponent));
        for (size_t row = 0; row < dimension_; ++row) {
            for (size_t column = 0; column <= row; ++column) {
                const auto value = matrix(row, column);
                if (value.is_zero()) continue;
                slong exponent = 0;
                const double mantissa = value.to_dbl_2exp(exponent);
                const slong difference = exponent - maximum_exponent;
                const double converted = difference < std::numeric_limits<int>::min()
                    ? 0.0
                    : std::scalbn(mantissa, static_cast<int>(difference));
                if (!std::isfinite(converted)) return;
                floating_matrix_(row, column) = converted;
                floating_matrix_(column, row) = converted;
            }
        }
    }

    floating_face analyze_floating()
    {
        const size_t cardinality = indices_.size();
        floating_solution_.assign(cardinality, 0.0);
        floating_products_.assign(dimension_, 0.0);
        if (cardinality == 1) {
            floating_solution_[0] = 1.0;
        } else {
            const size_t reduced_dimension = cardinality - 1;
            const size_t reference = indices_.back();
            floating_reduced_.resize(reduced_dimension, reduced_dimension);
            floating_rhs_.assign(reduced_dimension, 0.0);
            floating_pivots_.assign(reduced_dimension, 0);
            double maximum = 0.0;
            for (size_t row = 0; row < reduced_dimension; ++row) {
                const size_t original_row = indices_[row];
                floating_rhs_[row] = floating_matrix_(reference, reference) - floating_matrix_(original_row, reference);
                for (size_t column = 0; column <= row; ++column) {
                    const size_t original_column = indices_[column];
                    const double value = floating_matrix_(original_row, original_column)
                        - floating_matrix_(original_row, reference) - floating_matrix_(reference, original_column)
                        + floating_matrix_(reference, reference);
                    floating_reduced_(row, column) = value;
                    maximum = std::max(maximum, std::abs(value));
                }
            }
            if (!(maximum > 0.0) || !std::isfinite(maximum)) return {false, 0.0, true};
            const double inverse_scale = 1.0 / maximum;
            for (size_t row = 0; row < reduced_dimension; ++row) {
                floating_rhs_[row] *= inverse_scale;
                for (size_t column = 0; column <= row; ++column) floating_reduced_(row, column) *= inverse_scale;
            }
            if (!factor_and_forward_solve(floating_reduced_, reduced_dimension, floating_pivots_, floating_rhs_)
                || !solve_backward(floating_reduced_, reduced_dimension, floating_pivots_, floating_rhs_)) {
                return {false, 0.0, true};
            }
            double sum = 0.0;
            for (size_t row = 0; row < reduced_dimension; ++row) {
                floating_solution_[row] = floating_rhs_[row];
                sum += floating_rhs_[row];
            }
            floating_solution_.back() = 1.0 - sum;
        }

        for (size_t row = 0; row < dimension_; ++row) {
            for (size_t position = 0; position < cardinality; ++position)
                floating_products_[row] += floating_matrix_(row, indices_[position]) * floating_solution_[position];
            if (!std::isfinite(floating_products_[row])) return {false, 0.0, true};
        }
        floating_payoff_ = floating_products_[indices_.back()];
        double scale = std::max(1.0, std::abs(floating_payoff_));
        for (double value : floating_solution_) scale = std::max(scale, std::abs(value));
        for (double value : floating_products_) scale = std::max(scale, std::abs(value));
        const double tolerance = 256.0 * std::numeric_limits<double>::epsilon() * static_cast<double>(dimension_ + 1) * scale;
        bool has_zero = false;
        for (double value : floating_solution_) {
            if (value < -tolerance) return {false, tolerance, false};
            has_zero |= std::abs(value) <= tolerance;
        }
        const bool negative_witness_candidate = floating_payoff_ < -tolerance;
        if (has_zero) return {false, tolerance, false, negative_witness_candidate};
        for (size_t index = 0; index < dimension_; ++index) {
            if (std::binary_search(indices_.begin(), indices_.end(), index)) continue;
            if (floating_products_[index] < floating_payoff_ - tolerance)
                return {false, tolerance, false, negative_witness_candidate};
        }
        return {true, tolerance, false, negative_witness_candidate};
    }

    std::optional<std::vector<size_t>> floating_successor(const floating_face& face)
    {
        std::vector<size_t> candidates;
        for (size_t position = 0; position < indices_.size(); ++position)
            if (floating_solution_[position] < -face.tolerance) candidates.push_back(position);
        std::sort(candidates.begin(), candidates.end(), [&](size_t left, size_t right) {
            return floating_solution_[left] != floating_solution_[right]
                ? floating_solution_[left] < floating_solution_[right]
                : indices_[left] < indices_[right];
        });
        for (size_t removed : candidates) {
            std::vector<size_t> next = indices_;
            next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
            if (admissible(next)) return next;
        }
        if (!candidates.empty()) return std::nullopt;

        std::vector<size_t> nonzero;
        std::vector<size_t> zeros;
        nonzero.reserve(indices_.size());
        for (size_t position = 0; position < indices_.size(); ++position) {
            if (std::abs(floating_solution_[position]) <= face.tolerance) zeros.push_back(position);
            else nonzero.push_back(indices_[position]);
        }
        if (!zeros.empty()) {
            if (admissible(nonzero)) return nonzero;
            for (size_t removed : zeros) {
                std::vector<size_t> next = indices_;
                next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
                if (next != nonzero && admissible(next)) return next;
            }
            return std::nullopt;
        }

        candidates.clear();
        for (size_t index = 0; index < dimension_; ++index) {
            if (std::binary_search(indices_.begin(), indices_.end(), index)) continue;
            if (floating_products_[index] < floating_payoff_ - face.tolerance) candidates.push_back(index);
        }
        std::sort(candidates.begin(), candidates.end(), [&](size_t left, size_t right) {
            return floating_products_[left] != floating_products_[right]
                ? floating_products_[left] < floating_products_[right]
                : left < right;
        });
        for (size_t added : candidates) {
            std::vector<size_t> next = indices_;
            next.insert(std::lower_bound(next.begin(), next.end(), added), added);
            if (admissible(next)) return next;
        }
        return std::nullopt;
    }

    exact_face analyze_exact(const matrix_integer& matrix)
    {
        const size_t cardinality = indices_.size();
        exact_face face;
        solution_.resize(cardinality, 1);
        products_.resize(dimension_, 1);
        if (cardinality == 1) {
            denominator_.set_one();
            solution_(0, 0).set_one();
            payoff_ = matrix(indices_[0], indices_[0]);
        } else {
            const size_t reduced_dimension = cardinality - 1;
            const size_t reference = indices_.back();
            reduced_.resize(reduced_dimension, reduced_dimension);
            rhs_.resize(reduced_dimension, 1);
            for (size_t row = 0; row < reduced_dimension; ++row) {
                const size_t original_row = indices_[row];
                rhs_(row, 0).set_difference(matrix(reference, reference), matrix(original_row, reference));
                for (size_t column = 0; column <= row; ++column) {
                    const size_t original_column = indices_[column];
                    reduced_(row, column) = matrix(original_row, original_column);
                    reduced_(row, column) -= matrix(original_row, reference);
                    reduced_(row, column) -= matrix(reference, original_column);
                    reduced_(row, column) += matrix(reference, reference);
                }
            }
            face.nonsingular = factorization_.factorize_inplace(reduced_) != 0;
            face.nullity = reduced_dimension - factorization_.rank();
            if (face.nonsingular) {
                factorization_.solve_inplace(rhs_, denominator_, reduced_);
            } else {
                original_rhs_ = rhs_;
                face.consistent = factorization_.solve_consistent_inplace(rhs_, denominator_, reduced_);
                if (!face.consistent) return face;
            }
            integer sum;
            for (size_t row = 0; row < reduced_dimension; ++row) {
                solution_(row, 0) = rhs_(row, 0);
                sum += rhs_(row, 0);
            }
            solution_(cardinality - 1, 0).set_difference(denominator_, sum);
            payoff_.set_zero();
            for (size_t position = 0; position < cardinality; ++position)
                payoff_.addmul(matrix(reference, indices_[position]), solution_(position, 0));
        }

        face.feasible = true;
        for (size_t position = 0; position < cardinality; ++position)
            face.feasible &= solution_(position, 0).sign() >= 0;
        if (!face.feasible) return face;
        face.is_kkt = true;
        for (size_t row = 0; row < dimension_; ++row) {
            products_(row, 0).set_zero();
            for (size_t position = 0; position < cardinality; ++position)
                products_(row, 0).addmul(matrix(row, indices_[position]), solution_(position, 0));
            if (products_(row, 0).compare(payoff_) < 0) face.is_kkt = false;
        }
        return face;
    }

    std::optional<std::vector<size_t>> exact_nonsingular_successor(const exact_face& face)
    {
        std::vector<size_t> candidates;
        for (size_t position = 0; position < indices_.size(); ++position)
            if (solution_(position, 0).sign() < 0) candidates.push_back(position);
        std::sort(candidates.begin(), candidates.end(), [&](size_t left, size_t right) {
            const int comparison = solution_(left, 0).compare(solution_(right, 0));
            return comparison != 0 ? comparison < 0 : indices_[left] < indices_[right];
        });
        for (size_t removed : candidates) {
            std::vector<size_t> next = indices_;
            next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
            if (admissible(next)) return next;
        }
        if (!candidates.empty()) return std::nullopt;

        std::vector<size_t> nonzero;
        std::vector<size_t> zeros;
        for (size_t position = 0; position < indices_.size(); ++position) {
            if (solution_(position, 0).is_zero()) zeros.push_back(position);
            else nonzero.push_back(indices_[position]);
        }
        if (!zeros.empty()) {
            if (admissible(nonzero)) return nonzero;
            for (size_t removed : zeros) {
                std::vector<size_t> next = indices_;
                next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
                if (next != nonzero && admissible(next)) return next;
            }
            return std::nullopt;
        }
        if (face.is_kkt) return std::nullopt;

        for (size_t index = 0; index < dimension_; ++index)
            if (!std::binary_search(indices_.begin(), indices_.end(), index)
                && products_(index, 0).compare(payoff_) < 0) candidates.push_back(index);
        std::sort(candidates.begin(), candidates.end(), [&](size_t left, size_t right) {
            const int comparison = products_(left, 0).compare(products_(right, 0));
            return comparison != 0 ? comparison < 0 : left < right;
        });
        for (size_t added : candidates) {
            std::vector<size_t> next = indices_;
            next.insert(std::lower_bound(next.begin(), next.end(), added), added);
            if (admissible(next)) return next;
        }
        return std::nullopt;
    }

    std::optional<std::vector<size_t>> exact_singular_successor(const exact_face& face)
    {
        const size_t reduced_dimension = indices_.size() - 1;
        basis_.resize(reduced_dimension, face.nullity);
        factorization_.nullspace_basis(basis_, reduced_);
        if (face.consistent) {
            for (size_t column = 0; column < face.nullity; ++column) {
                if (auto next = boundary_support(column, 1)) return next;
                if (auto next = boundary_support(column, -1)) return next;
            }
            return std::nullopt;
        }
        bool found_separating_direction = false;
        for (size_t column = 0; column < face.nullity; ++column) {
            integer dot;
            for (size_t row = 0; row < reduced_dimension; ++row) dot.addmul(basis_(row, column), original_rhs_(row, 0));
            if (dot.is_zero()) continue;
            found_separating_direction = true;
            if (auto next = boundary_support(column, dot.sign() > 0 ? 1 : -1)) return next;
        }
        if (found_separating_direction) return std::nullopt;
        throw std::logic_error("an inconsistent symmetric KKT system has no separating nullspace direction");
    }

    std::optional<std::vector<size_t>> boundary_support(size_t column, int orientation)
    {
        direction_.assign(indices_.size(), integer{});
        for (size_t row = 0; row + 1 < indices_.size(); ++row) {
            direction_[row] = basis_(row, column);
            if (orientation < 0) direction_[row].negate();
            direction_.back() -= direction_[row];
        }
        std::optional<size_t> minimum;
        for (size_t position = 0; position < direction_.size(); ++position) {
            if (direction_[position].sign() >= 0) continue;
            if (!minimum || direction_[position].compare(direction_[*minimum]) < 0) minimum = position;
        }
        if (!minimum) return std::nullopt;
        std::vector<size_t> next;
        for (size_t position = 0; position < indices_.size(); ++position)
            if (direction_[position].compare(direction_[*minimum]) != 0) next.push_back(indices_[position]);
        return admissible(next) ? std::optional{std::move(next)} : std::nullopt;
    }

    void write_support(const std::vector<size_t>& indices, support& result)
    {
        support_context_.clear(result);
        for (size_t index : indices) support_context_.set(result, index);
    }

    bool admissible(const std::vector<size_t>& indices)
    {
        if (indices.empty()) return false;
        write_support(indices, candidate_support_);
        return visited_.find(candidate_support_) == visited_.end();
    }

    void move_to(std::vector<size_t> next)
    {
        indices_ = std::move(next);
        write_support(indices_, candidate_support_);
        visited_.insert(support_context_.clone(candidate_support_));
    }

    support_context support_context_;
    size_t dimension_ = 0;
    std::vector<size_t> indices_;
    support candidate_support_;
    std::set<support, support_less> visited_;
    double_matrix floating_matrix_;
    double_matrix floating_reduced_;
    std::vector<double> floating_rhs_;
    std::vector<double> floating_solution_;
    std::vector<double> floating_products_;
    std::vector<int> floating_pivots_;
    double floating_payoff_ = 0.0;
    fraction_free_ldlt_factorization factorization_;
    matrix_integer reduced_;
    matrix_integer rhs_;
    matrix_integer original_rhs_;
    matrix_integer solution_;
    matrix_integer products_;
    matrix_integer basis_;
    integer denominator_;
    integer payoff_;
    std::vector<integer> direction_;
};

heuristic_kkt_result run_heuristic_kkt_search(const matrix_integer& matrix)
{
    heuristic_kkt_search search(matrix.rows());
    auto result = search.run(matrix);
    if (diagnostics::enabled()) {
        std::ostringstream event;
        event << "event=heuristic_kkt_search visited=" << result.visited
              << " exact_continuation=" << (result.exact_continuation ? "yes" : "no")
              << " reached_kkt=" << (result.reached_kkt ? "yes" : "no") << " value_sign=";
        if (result.sign) event << *result.sign;
        else event << "unresolved";
        diagnostics::record_event(event.str());
    }
    return {std::move(result.sign), result.visited, result.exact_continuation, result.reached_kkt};
}

} // namespace coposit::pre_check::detail
