#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

struct positive_ratio {
    integer numerator;
    integer denominator;
};

struct breakpoint_event {
    positive_ratio root;
    bool solution_entry = false;
    int direction_sign = 0;
};

bool ratio_less(const positive_ratio& left, const positive_ratio& right)
{
    integer left_product;
    integer right_product;
    left_product.set_product(left.numerator, right.denominator);
    right_product.set_product(right.numerator, left.denominator);
    return left_product.compare(right_product) < 0;
}

bool ratio_equal(const positive_ratio& left, const positive_ratio& right)
{
    return !ratio_less(left, right) && !ratio_less(right, left);
}

bool negative_orientation_has_larger_upper(size_t positive_products, size_t negative_products) noexcept
{
    return negative_products > positive_products;
}

class double_matrix {
public:
    void resize(size_t rows, size_t columns)
    {
        rows_ = rows;
        columns_ = columns;
        values_.assign(rows * columns, 0.0);
    }

    size_t rows() const noexcept { return rows_; }
    double& operator()(size_t row, size_t column) noexcept { return values_[row * columns_ + column]; }
    double operator()(size_t row, size_t column) const noexcept { return values_[row * columns_ + column]; }

private:
    size_t rows_ = 0;
    size_t columns_ = 0;
    std::vector<double> values_;
};

constexpr double bunch_kaufman_alpha = 0.6403882032022076; // (1 + sqrt(17)) / 8
constexpr double floating_pivot_cutoff = 64.0 * std::numeric_limits<double>::epsilon();

/*
 * The pivoted symmetric factorization below is the lower-triangle, one-right-hand-side subset of LAPACK's DSYTF2/DSYTRS path,
 * adapted from FracESSA's fast candidate filter. It is used only to propose a path. No floating result is a certificate.
 *
 * Copyright (c) 1992-2023 The University of Tennessee and The University of Tennessee Research Foundation. All rights reserved.
 * Copyright (c) 2000-2023 The University of California Berkeley. All rights reserved.
 * Copyright (c) 2006-2023 The University of Colorado Denver. All rights reserved.
 *
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
void swap_active_coordinates(
    double_matrix& system, size_t active_start, size_t first, size_t second, size_t dimension, size_t pivot_size)
{
    if (first == second) return;
    for (size_t row = second + 1; row < dimension; ++row) std::swap(system(row, first), system(row, second));
    for (size_t row = first + 1; row < second; ++row) std::swap(system(row, first), system(second, row));
    std::swap(system(first, first), system(second, second));
    if (pivot_size == 2) std::swap(system(active_start + 1, active_start), system(second, active_start));
}

bool factor_and_forward_solve_bunch_kaufman(
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

bool solve_bunch_kaufman_backward(
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

#ifdef COPOSIT_XXX_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
#endif

class timeout_terminator final : public CaDiCaL::Terminator {
public:
    bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
    explicit interval_sat(size_t dimension)
        : dimension_(dimension)
    {
        if (dimension_ > static_cast<size_t>(std::numeric_limits<int>::max() - 1))
            throw std::overflow_error("SAT variable count exceeds CaDiCaL's integer literal range");
        next_variable_ = static_cast<int>(dimension_) + 1;

        if (!solver_.configure("sat")) throw std::runtime_error("CaDiCaL lacks its satisfiable-instance configuration");
        if (!solver_.set("ilb", 2)) throw std::runtime_error("CaDiCaL lacks incremental lazy backtracking");
        solver_.connect_terminator(&terminator_);

        std::vector<int> wires;
        size_t padded_dimension = 1;
        while (padded_dimension < dimension_) {
            if (padded_dimension > static_cast<size_t>(std::numeric_limits<int>::max()) / 2)
                throw std::overflow_error("SAT cardinality network is too large");
            padded_dimension *= 2;
        }
        wires.reserve(padded_dimension);
        for (size_t index = 0; index < dimension_; ++index) wires.push_back(variable(index));

        if (padded_dimension != dimension_) {
            const int constant_false = new_variable();
            add_clause({-constant_false});
            wires.resize(padded_dimension, constant_false);
        }

        bitonic_sort(wires, 0, wires.size(), true);
        cardinality_outputs_.assign(wires.begin(), wires.begin() + dimension_);
    }

    void start_cardinality(size_t cardinality) noexcept { cardinality_ = cardinality; }

    bool take_first(std::vector<size_t>& indices)
    {
        assert(cardinality_ >= 1 && cardinality_ <= dimension_);
        solver_.assume(cardinality_outputs_[cardinality_ - 1]);
        if (cardinality_ < dimension_) solver_.assume(-cardinality_outputs_[cardinality_]);

        const int status = solver_.solve();
        if (status == CaDiCaL::UNSATISFIABLE) return false;
        if (status != CaDiCaL::SATISFIABLE) {
            timeout_checkpoint();
            throw std::runtime_error("CaDiCaL returned an inconclusive result without a coposit timeout");
        }

        indices.clear();
        for (size_t index = 0; index < dimension_; ++index)
            if (solver_.val(variable(index)) > 0) indices.push_back(index);
        assert(indices.size() == cardinality_);
        return true;
    }

    bool available(const std::vector<size_t>& indices)
    {
        size_t selected = 0;
        for (size_t index = 0; index < dimension_; ++index) {
            const bool contains = selected < indices.size() && indices[selected] == index;
            solver_.assume(contains ? variable(index) : -variable(index));
            selected += contains;
        }
        assert(selected == indices.size());

        const int status = solver_.solve();
        if (status == CaDiCaL::UNSATISFIABLE) return false;
        if (status == CaDiCaL::SATISFIABLE) return true;
        timeout_checkpoint();
        throw std::runtime_error("CaDiCaL returned an inconclusive result without a coposit timeout");
    }

    void add_interval(const support& lower, const support& upper)
    {
        size_t upper_size = 0;
#ifdef COPOSIT_XXX_TESTING
        last_interval_clause_size_ = 0;
#endif
        for (size_t index = 0; index < dimension_; ++index) {
            const bool in_upper = upper.contains(index);
            if (in_upper) ++upper_size;
            if (lower.contains(index)) {
                solver_.add(-variable(index));
#ifdef COPOSIT_XXX_TESTING
                ++last_interval_clause_size_;
#endif
            } else if (!in_upper) {
                solver_.add(variable(index));
#ifdef COPOSIT_XXX_TESTING
                ++last_interval_clause_size_;
#endif
            }
        }
        if (upper_size < dimension_) {
            solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_XXX_TESTING
            ++last_interval_clause_size_;
#endif
        }
        solver_.add(0);
    }

#ifdef COPOSIT_XXX_TESTING
    size_t last_interval_clause_size() const noexcept { return last_interval_clause_size_; }
#endif
private:
    int variable(size_t index) const noexcept { return static_cast<int>(index) + 1; }

    int new_variable()
    {
        if (next_variable_ == std::numeric_limits<int>::max())
            throw std::overflow_error("SAT cardinality network exceeds CaDiCaL's integer literal range");
        if ((next_variable_ & 4095) == 0) timeout_checkpoint();
        return next_variable_++;
    }

    void add_clause(std::initializer_list<int> literals)
    {
        for (const int literal : literals) solver_.add(literal);
        solver_.add(0);
    }

    std::pair<int, int> comparator(int first, int second)
    {
        const int high = new_variable();
        const int low = new_variable();
        add_clause({-first, high});
        add_clause({-second, high});
        add_clause({first, second, -high});
        add_clause({first, -low});
        add_clause({second, -low});
        add_clause({-first, -second, low});
        return {high, low};
    }

    void compare_exchange(std::vector<int>& wires, size_t first, size_t second, bool descending)
    {
        const auto [high, low] = comparator(wires[first], wires[second]);
        wires[first] = descending ? high : low;
        wires[second] = descending ? low : high;
    }

    void bitonic_merge(std::vector<int>& wires, size_t first, size_t count, bool descending)
    {
        if (count < 2) return;
        const size_t half = count / 2;
        for (size_t index = first; index < first + half; ++index)
            compare_exchange(wires, index, index + half, descending);
        bitonic_merge(wires, first, half, descending);
        bitonic_merge(wires, first + half, half, descending);
    }

    void bitonic_sort(std::vector<int>& wires, size_t first, size_t count, bool descending)
    {
        if (count < 2) return;
        const size_t half = count / 2;
        bitonic_sort(wires, first, half, !descending);
        bitonic_sort(wires, first + half, half, descending);
        bitonic_merge(wires, first, count, descending);
    }

    size_t dimension_;
    int next_variable_ = 1;
    size_t cardinality_ = 0;
#ifdef COPOSIT_XXX_TESTING
    size_t last_interval_clause_size_ = 0;
#endif
    timeout_terminator terminator_;
    CaDiCaL::Solver solver_;
    std::vector<int> cardinality_outputs_;
};

struct coverage_score {
    size_t width = 0;
    size_t upper_size = 0;
};

bool better_score(const coverage_score& candidate, const coverage_score& current) noexcept
{
    return candidate.upper_size > current.upper_size
        || (candidate.upper_size == current.upper_size && candidate.width > current.width);
}

bool better_ray_candidate(const coverage_score& candidate, size_t gains, size_t losses, bool current_initialized,
                          const coverage_score& current, size_t current_gains, size_t current_losses) noexcept
{
    if (!current_initialized || better_score(candidate, current)) return true;
    if (candidate.upper_size != current.upper_size || candidate.width != current.width) return false;
    return gains > current_gains || (gains == current_gains && losses < current_losses);
}

constexpr size_t maximum_ray_shortlist = 64;

size_t ray_shortlist_limit(size_t matrix_dimension, size_t support_dimension)
{
    const size_t dimension_budget = static_cast<size_t>(std::ceil(3.0L * std::sqrt(static_cast<long double>(matrix_dimension))));
    return std::min({support_dimension, maximum_ray_shortlist, dimension_budget});
}

struct ray_candidate {
    positive_ratio step;
    coverage_score score;
    size_t direction = 0;
    size_t gains = 0;
    size_t losses = 0;
};

bool better_shortlist_candidate(const ray_candidate& candidate, const ray_candidate& current) noexcept
{
    if (candidate.gains != current.gains) return candidate.gains > current.gains;
    if (candidate.losses != current.losses) return candidate.losses < current.losses;
    if (candidate.score.upper_size != current.score.upper_size) return candidate.score.upper_size > current.score.upper_size;
    if (candidate.score.width != current.score.width) return candidate.score.width > current.score.width;
    return candidate.direction < current.direction;
}

struct pair_score {
    size_t union_gains = 0;
    size_t common_losses = 0;
    size_t total_gains = 0;
    size_t union_losses = 0;
};

struct ray_pair {
    size_t first = 0;
    size_t second = 0;
    pair_score score;
    bool initialized = false;
};

struct face_result {
    bool nonsingular = true;
    bool consistent = true;
    bool positive_semidefinite = true;
    bool feasible = false;
    bool is_kkt = false;
    size_t nullity = 0;
};

struct floating_face_result {
    bool terminal_candidate = false;
    double tolerance = 0.0;
    bool inconclusive = false;
    bool negative_witness_candidate = false;
};

struct path_frame {
    std::vector<size_t> indices;
    std::vector<std::vector<size_t>> successors;
    size_t next_successor = 0;
    bool expanded = false;
};

struct blocked_successors {
    size_t current_path = 0;
    size_t known_path = 0;
    size_t sat = 0;
    size_t empty = 0;
};

enum class successor_state : std::uint8_t { available, current_path, known_path, sat, empty };

struct pending_interval {
    support lower;
    support upper;
};

struct buffered_interval {
    support lower;
    support upper;
    bool strict_safe = true;
};

bool better_pair(const ray_pair& candidate, const ray_pair& current) noexcept
{
    if (!current.initialized) return true;
    if (candidate.score.union_gains != current.score.union_gains)
        return candidate.score.union_gains > current.score.union_gains;
    if (candidate.score.common_losses != current.score.common_losses)
        return candidate.score.common_losses < current.score.common_losses;
    if (candidate.score.total_gains != current.score.total_gains)
        return candidate.score.total_gains > current.score.total_gains;
    if (candidate.score.union_losses != current.score.union_losses)
        return candidate.score.union_losses < current.score.union_losses;
    return std::pair{candidate.first, candidate.second} < std::pair{current.first, current.second};
}

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : dimension_(dimension)
        , factorization_(dimension)
        , kkt_factorization_(dimension > 0 ? dimension - 1 : 0)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        completed_path_supports_.resize(dimension + 1);
        indices_.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.emplace_back(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : dimension_(dimension)
        , factorization_(dimension)
        , kkt_factorization_(dimension > 0 ? dimension - 1 : 0)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        completed_path_supports_.resize(dimension + 1);
        indices_.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.emplace_back(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_XXX_TESTING
        optimized_certificate_count_ = 0;
        combined_ray_sweep_count_ = 0;
        combined_ray_improvement_count_ = 0;
        last_committed_path_interval_count_ = 0;
        last_path_reached_kkt_ = false;
#endif
        prepare_floating_matrix(matrix);
        supports_.emplace(matrix.rows());
        std::vector<size_t> seed;
        size_t lower_cardinality = 1;
        size_t upper_cardinality = dimension_;
        bool take_lower = true;
        while (lower_cardinality <= upper_cardinality) {
            size_t& cardinality = take_lower ? lower_cardinality : upper_cardinality;
            supports_->start_cardinality(cardinality);
            while (!supports_->take_first(seed)) {
                completed_path_supports_[cardinality].clear();
                if (take_lower) ++lower_cardinality;
                else --upper_cardinality;
                if (lower_cardinality > upper_cardinality) break;
                supports_->start_cardinality(cardinality);
            }
            if (lower_cardinality > upper_cardinality) break;
            if (!process_path(matrix, seed)) {
                diagnostics_.finish();
                return false;
            }
            take_lower = !take_lower;
        }

        diagnostics_.finish();
#ifdef COPOSIT_XXX_TESTING
        publish_test_counters();
#endif
        return true;
    }

    bool process_path(const matrix_integer& matrix, const std::vector<size_t>& seed, bool exact_from_start = false)
    {
        path_intervals_.clear();
        path_visited_.clear();
        indices_ = seed;
        const size_t path_id = next_path_id_++;
        if (diagnostics_.active()) {
            std::ostringstream event;
            event << "event=xxx_two_path_seed path=" << path_id << " support=";
            append_support(event, seed);
            diagnostics::record_event(event.str());
        }
        if (contains(completed_path_supports_, make_support(seed), seed.size())) {
            if (!certify_seed_if_open(matrix, seed, path_id)) return false;
            record_path_event(path_id, "known_seed_certified", 0);
            return true;
        }

        std::vector<path_frame> stack;
        stack.push_back({seed});
        remember_current_path(seed);
        bool critical_mode = exact_from_start;
        while (!stack.empty()) {
            timeout_checkpoint();
            path_frame& frame = stack.back();
            indices_ = frame.indices;
            if (!frame.expanded) {
                frame.expanded = true;
                diagnostics_.stage(indices_.size());
                diagnostics_.visit_support();
                diagnostics_.secondary();
                const support exact = make_support(indices_);
                const bool exact_step = critical_mode;
                const floating_face_result floating = exact_step ? floating_face_result{} : analyze_floating_kkt();
                std::optional<face_result> exact_face;
                if (exact_step || (!floating.inconclusive && (floating.negative_witness_candidate || floating.terminal_candidate)))
                    exact_face = analyze_kkt(matrix);
                if (exact_face && exact_face->consistent && exact_face->feasible
                    && kkt_payoff_.sign() < 0) {
                    record_face_stationary_event(*exact_face);
                    record_terminal_event("intermediate_negative");
                    return false;
                }
                if (exact_step || (!floating.inconclusive && floating.terminal_candidate)) {
                    const face_result& face = *exact_face;
                    if (face.consistent && face.feasible && face.is_kkt) {
                        if (!process_kkt_result(exact, face)) {
                            record_terminal_event("kkt_negative");
                            return false;
                        }
#ifdef COPOSIT_XXX_TESTING
                        last_path_reached_kkt_ = true;
#endif
                        commit_path_intervals();
                        const std::vector<size_t> terminal = indices_;
                        if (!certify_seed_if_open(matrix, seed, path_id)) return false;
                        indices_ = terminal;
                        record_path_event(path_id, "verified_kkt", path_visited_.size());
                        return true;
                    }
                    if (!exact_step) {
                        critical_mode = true;
                        record_path_event(path_id, "critical_point", stack.size());
                    }
                    std::optional<std::vector<size_t>> exact_next = face.nonsingular
                        ? nonsingular_successor(exact, face)
                        : singular_successor(face);
                    if (exact_next) frame.successors.push_back(std::move(*exact_next));
                }
                if (!exact_step && !floating.inconclusive && !floating.terminal_candidate)
                    frame.successors = floating_successors(exact, floating);
            }

            bool descended = false;
            while (frame.next_successor < frame.successors.size()) {
                std::vector<size_t> next = frame.successors[frame.next_successor++];
                if (successor_status(next) != successor_state::available) continue;
                record_path_step(path_id, frame.indices, next);
                remember_current_path(next);
                stack.push_back({std::move(next)});
                descended = true;
                break;
            }
            if (descended) continue;

            record_path_backtrack(path_id, frame.indices);
            stack.pop_back();
        }

        if (!certify_seed_if_open(matrix, seed, path_id)) return false;
        indices_ = seed;
        record_path_event(path_id, "root_exhausted", path_visited_.size());
        return true;
    }

#ifdef COPOSIT_XXX_TESTING
    size_t exact_random_paths_for_testing(const matrix_integer& matrix, size_t requested, uint64_t random_seed)
    {
        prepare_floating_matrix(matrix);
        supports_.emplace(matrix.rows());
        std::mt19937_64 random(random_seed);
        std::uniform_int_distribution<size_t> cardinality(1, dimension_);
        std::vector<size_t> seed(dimension_);
        std::iota(seed.begin(), seed.end(), 0);

        size_t processed = 0;
        for (size_t attempts = 0; processed < requested && attempts < requested * 1000; ++attempts) {
            std::shuffle(seed.begin(), seed.end(), random);
            seed.resize(cardinality(random));
            std::sort(seed.begin(), seed.end());
            if (!supports_->available(seed) || contains(completed_path_supports_, make_support(seed), seed.size())) {
                seed.resize(dimension_);
                std::iota(seed.begin(), seed.end(), 0);
                continue;
            }
            ++processed;
            if (!process_path(matrix, seed, true)) break;
            seed.resize(dimension_);
            std::iota(seed.begin(), seed.end(), 0);
        }
        return processed;
    }

    std::array<size_t, 4> buffered_path_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed)
    {
        prepare_floating_matrix(matrix);
        supports_.emplace(matrix.rows());
        last_committed_path_interval_count_ = 0;
        last_path_reached_kkt_ = false;
        if (!process_path(matrix, seed)) return {};
        return {path_visited_.size(), last_committed_path_interval_count_, last_path_reached_kkt_, supports_->available(seed)};
    }

    bool check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        support lower(matrix.rows());
        support upper(matrix.rows());
        const bool result = optimize_support_for_testing(matrix, indices, lower, upper);
        last_fixed_support_upper_size = 0;
        for (size_t index = 0; index < matrix.rows(); ++index) last_fixed_support_upper_size += upper.contains(index);
        return result;
    }

    bool optimize_support_for_testing(
        const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
    {
        optimized_certificate_count_ = 0;
        combined_ray_sweep_count_ = 0;
        combined_ray_improvement_count_ = 0;
        indices_ = indices;
        captured_lower_ = &lower;
        captured_upper_ = &upper;
        const bool result = process_subset(matrix);
        captured_lower_ = nullptr;
        captured_upper_ = nullptr;
        publish_test_counters();
        return result;
    }

    std::vector<size_t> floating_successor_for_testing(const matrix_integer& matrix, const std::vector<size_t>& current,
                                                       const std::vector<std::vector<size_t>>& known)
    {
        prepare_floating_matrix(matrix);
        supports_.emplace(matrix.rows());
        for (auto& bucket : completed_path_supports_) bucket.clear();
        for (const auto& path_support : known) remember(completed_path_supports_, path_support);
        indices_ = current;
        path_visited_.clear();
        remember_current_path(current);
        const floating_face_result face = analyze_floating_kkt();
        if (face.terminal_candidate) return {};
        blocked_successors blocked;
        auto next = floating_successor(make_support(current), face, blocked);
        return next ? *next : std::vector<size_t>{};
    }

    void completed_path_seed_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed)
    {
        prepare_floating_matrix(matrix);
        supports_.emplace(matrix.rows());
        remember(completed_path_supports_, seed);
        static_cast<void>(process_path(matrix, seed));
    }
#endif

private:
    using support_buckets = std::vector<std::set<support>>;

    static bool contains(const support_buckets& buckets, const support& value, size_t cardinality)
    {
        const auto& bucket = buckets[cardinality];
        return bucket.find(value) != bucket.end();
    }

    void remember(support_buckets& buckets, const std::vector<size_t>& indices)
    {
        buckets[indices.size()].insert(make_support(indices));
    }

    void remember_current_path(const std::vector<size_t>& indices)
    {
        support value = make_support(indices);
        path_visited_.insert(value);
        completed_path_supports_[indices.size()].insert(std::move(value));
    }

    void prepare_floating_matrix(const matrix_integer& matrix)
    {
        floating_game_.resize(dimension_, dimension_);
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
                if (!std::isfinite(converted)) throw std::runtime_error("XXX_TWO floating matrix conversion is not finite");
                floating_game_(row, column) = converted;
                floating_game_(column, row) = converted;
            }
        }
    }

    floating_face_result analyze_floating_kkt()
    {
        const size_t cardinality = indices_.size();
        floating_solution_.assign(cardinality, 0.0);
        floating_products_.assign(dimension_, 0.0);
        floating_payoff_ = 0.0;

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
                floating_rhs_[row] = floating_game_(reference, reference) - floating_game_(original_row, reference);
                for (size_t column = 0; column <= row; ++column) {
                    const size_t original_column = indices_[column];
                    const double value = floating_game_(original_row, original_column)
                        - floating_game_(original_row, reference) - floating_game_(reference, original_column)
                        + floating_game_(reference, reference);
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
            if (!factor_and_forward_solve_bunch_kaufman(
                    floating_reduced_, reduced_dimension, floating_pivots_, floating_rhs_)
                || !solve_bunch_kaufman_backward(
                    floating_reduced_, reduced_dimension, floating_pivots_, floating_rhs_)) {
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
            double value = 0.0;
            for (size_t position = 0; position < cardinality; ++position)
                value += floating_game_(row, indices_[position]) * floating_solution_[position];
            if (!std::isfinite(value)) return {false, 0.0, true};
            floating_products_[row] = value;
        }
        floating_payoff_ = floating_products_[indices_.back()];

        double scale = std::max(1.0, std::abs(floating_payoff_));
        for (double value : floating_solution_) scale = std::max(scale, std::abs(value));
        for (double value : floating_products_) scale = std::max(scale, std::abs(value));
        const double tolerance = 256.0 * std::numeric_limits<double>::epsilon()
            * static_cast<double>(dimension_ + 1) * scale;
        bool has_zero_coordinate = false;
        for (double value : floating_solution_) {
            if (value < -tolerance) return {false, tolerance};
            has_zero_coordinate |= std::abs(value) <= tolerance;
        }
        const bool negative_witness_candidate = floating_payoff_ < -tolerance;
        if (has_zero_coordinate) return {false, tolerance, false, negative_witness_candidate};
        for (size_t index = 0; index < dimension_; ++index) {
            if (std::binary_search(indices_.begin(), indices_.end(), index)) continue;
            if (floating_products_[index] < floating_payoff_ - tolerance)
                return {false, tolerance, false, negative_witness_candidate};
        }
        return {true, tolerance, false, negative_witness_candidate};
    }

    successor_state successor_status(const std::vector<size_t>& candidate)
    {
        if (candidate.empty()) return successor_state::empty;
        const support candidate_support = make_support(candidate);
        if (path_visited_.find(candidate_support) != path_visited_.end()) return successor_state::current_path;
        if (contains(completed_path_supports_, candidate_support, candidate.size())) return successor_state::known_path;
        if (!supports_->available(candidate)) return successor_state::sat;
        return successor_state::available;
    }

    static void count_block(successor_state state, blocked_successors& blocked) noexcept
    {
        switch (state) {
        case successor_state::available: break;
        case successor_state::current_path: ++blocked.current_path; break;
        case successor_state::known_path: ++blocked.known_path; break;
        case successor_state::sat: ++blocked.sat; break;
        case successor_state::empty: ++blocked.empty; break;
        }
    }

    bool accept_successor(std::vector<size_t>& candidate, blocked_successors& blocked)
    {
        const successor_state state = successor_status(candidate);
        if (state == successor_state::available) return true;
        count_block(state, blocked);
        return false;
    }

    std::vector<std::vector<size_t>> floating_successors(const support& exact, const floating_face_result& face)
    {
        std::vector<std::vector<size_t>> successors;
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
            successors.push_back(std::move(next));
        }
        if (!candidates.empty()) return successors;

        std::vector<size_t> nonzero;
        std::vector<size_t> zeros;
        nonzero.reserve(indices_.size());
        for (size_t position = 0; position < indices_.size(); ++position) {
            if (std::abs(floating_solution_[position]) <= face.tolerance)
                zeros.push_back(position);
            else
                nonzero.push_back(indices_[position]);
        }
        if (!zeros.empty()) {
            successors.push_back(nonzero);
            for (size_t removed : zeros) {
                std::vector<size_t> next = indices_;
                next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
                if (next != nonzero) successors.push_back(std::move(next));
            }
            return successors;
        }

        for (size_t index = 0; index < dimension_; ++index) {
            if (exact.contains(index)) continue;
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
            successors.push_back(std::move(next));
        }
        return successors;
    }

    std::optional<std::vector<size_t>> floating_successor(
        const support& exact, const floating_face_result& face, blocked_successors& blocked)
    {
        for (std::vector<size_t>& candidate : floating_successors(exact, face)) {
            if (accept_successor(candidate, blocked)) return std::move(candidate);
        }
        return std::nullopt;
    }

    static void append_support(std::ostringstream& event, const std::vector<size_t>& indices)
    {
        event << '[';
        for (size_t position = 0; position < indices.size(); ++position) {
            if (position != 0) event << ',';
            event << indices[position] + 1;
        }
        event << ']';
    }

    void record_path_step(size_t path_id, const std::vector<size_t>& current, const std::vector<size_t>& next) const
    {
        if (!diagnostics_.active()) return;
        std::ostringstream event;
        event << "event=xxx_two_path_step path=" << path_id << " support=";
        append_support(event, current);
        event << " next=";
        append_support(event, next);
        diagnostics::record_event(event.str());
    }

    void record_seed_certificate_event(size_t path_id, const std::vector<size_t>& seed) const
    {
        if (!diagnostics_.active()) return;
        std::ostringstream event;
        event << "event=xxx_two_seed_certificate path=" << path_id << " support=";
        append_support(event, seed);
        diagnostics::record_event(event.str());
    }

    void record_path_backtrack(size_t path_id, const std::vector<size_t>& support_indices) const
    {
        if (!diagnostics_.active()) return;
        std::ostringstream event;
        event << "event=xxx_two_path_backtrack path=" << path_id << " support=";
        append_support(event, support_indices);
        diagnostics::record_event(event.str());
    }

    bool certify_seed_if_open(const matrix_integer& matrix, const std::vector<size_t>& seed, size_t path_id)
    {
        if (!supports_->available(seed)) return true;
        record_seed_certificate_event(path_id, seed);
        indices_ = seed;
        if (!process_subset(matrix)) {
            record_terminal_event("seed_certificate_negative");
            return false;
        }
        commit_path_intervals();
        if (supports_->available(seed)) fail_path(path_id, "seed_certificate_did_not_cover_seed", {});
        return true;
    }

    void record_path_event(size_t path_id, const char* outcome, size_t length) const
    {
        if (!diagnostics_.active()) return;
        std::ostringstream event;
        event << "event=xxx_two_path path=" << path_id << " outcome=" << outcome << " length=" << length << " support=";
        append_support(event, indices_);
        diagnostics::record_event(event.str());
    }

    [[noreturn]] void fail_path(size_t path_id, const char* reason, const blocked_successors& blocked) const
    {
        if (diagnostics_.active()) {
            std::ostringstream event;
            event << "event=xxx_two_path path=" << path_id << " outcome=error reason=" << reason
                  << " blocked_current=" << blocked.current_path << " blocked_known=" << blocked.known_path
                  << " blocked_sat=" << blocked.sat << " blocked_empty=" << blocked.empty << " support=";
            append_support(event, indices_);
            diagnostics::record_event(event.str());
        }
        std::ostringstream message;
        message << "XXX_TWO KKT path error: " << reason << " at support cardinality " << indices_.size()
                << " (blocked current=" << blocked.current_path << ", known=" << blocked.known_path
                << ", SAT=" << blocked.sat << ", empty=" << blocked.empty << ')';
        throw std::runtime_error(message.str());
    }

    bool process_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (singular && diagnostics_.active()) diagnostics_.singular_support(dimension - factorization_.rank());
        if (singular) return process_singular_subset(matrix);
        return process_nonsingular_subset(matrix);
    }

    bool process_singular_subset(const matrix_integer& matrix)
    {
        factorization_.one_nullspace_vector(solution_, principal_);

        bool has_positive_entry = false;
        bool has_negative_entry = false;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            has_positive_entry |= solution_(row, 0).sign() > 0;
            has_negative_entry |= solution_(row, 0).sign() < 0;
        }
        assert(has_positive_entry || has_negative_entry);
        if (!has_positive_entry) {
            solution_.negate();
            has_negative_entry = false;
        }

        if (!has_negative_entry && !record_zero_witness()) return false;

        calculate_product(matrix, solution_, 0, product_);
        if (has_negative_entry) {
            size_t positive_products = 0;
            size_t negative_products = 0;
            for (const integer& value : product_) {
                positive_products += value.sign() > 0;
                negative_products += value.sign() < 0;
            }
            if (negative_orientation_has_larger_upper(positive_products, negative_products)) {
                solution_.negate();
                for (integer& value : product_) value.negate();
            }
        }
        return add_certificate();
    }

    bool process_nonsingular_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);
        if (all_nonpositive(solution_, 0)) return false;

        calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
        current_score_ = score(solution_, 0, product_);
        if (dimension > 1 && current_score_.width + 1 < matrix.rows()) {
            directions_.resize(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    if (row == column) directions_(row, column).set_one();
                    else directions_(row, column).set_zero();
                }
            }

            integer direction_denominator;
            factorization_.solve_inplace(directions_, direction_denominator, principal_);
            assert(direction_denominator.compare(denominator) == 0);

            direction_products_.resize(matrix.rows(), dimension);

            bool first_pass = true;
            bool pass_improved = false;
            do {
                pass_improved = false;
                ray_shortlist_.clear();
                for (size_t direction = 0; direction < dimension; ++direction) {
                    if (first_pass)
                        calculate_nonsingular_product(
                            matrix, directions_, direction, direction_denominator, direction_products_, direction);
                    bool improved = false;
                    if (!optimize_direction(direction, improved, true)) return false;
                    pass_improved |= improved;
                    if (current_score_.width + 1 == matrix.rows()) break;
                }
                first_pass = false;
                if (!pass_improved && current_score_.width + 1 < matrix.rows() && !try_combined_rays()) return false;
            } while (pass_improved && current_score_.width + 1 < matrix.rows());
        }

        return add_certificate();
    }

    bool search_direction(size_t direction, bool remember_ray)
    {
        best_score_ = current_score_;
        best_numerator_.set_zero();
        best_denominator_.set_one();
        ray_best_initialized_ = false;
        ray_best_score_ = {};
        ray_best_numerator_.set_zero();
        ray_best_denominator_.set_one();
        ray_best_gains_ = 0;
        ray_best_losses_ = 0;
        negative_witness_found_ = false;

        find_breakpoints(direction);
        if (breakpoint_events_.empty()) return true;
        sweep_direction(direction);
        if (negative_witness_found_) return false;
        if (remember_ray && ray_best_initialized_ && ray_best_gains_ > 0) retain_ray_candidate(direction);
        return true;
    }

    bool optimize_direction(size_t direction, bool& improved, bool remember_ray)
    {
        improved = false;
        if (!search_direction(direction, remember_ray)) return false;
        if (best_numerator_.is_zero()) return true;

        apply_candidate(direction, best_numerator_, best_denominator_);
        current_score_ = best_score_;
        improved = true;
#ifdef COPOSIT_XXX_TESTING
        ++optimized_certificate_count_;
#endif
        return true;
    }

    void find_breakpoints(size_t direction)
    {
        breakpoint_events_.clear();
        for (size_t row = 0; row < solution_.rows(); ++row)
            add_positive_breakpoint(solution_(row, 0), directions_(row, direction), true);
        for (size_t row = 0; row < product_.size(); ++row)
            add_positive_breakpoint(product_[row], direction_products_(row, direction), false);

        std::sort(breakpoint_events_.begin(), breakpoint_events_.end(), [](const auto& left, const auto& right) {
            return ratio_less(left.root, right.root);
        });
    }

    void add_positive_breakpoint(integer::const_reference base, integer::const_reference direction, bool solution_entry)
    {
        if (base.is_zero() || direction.is_zero() || base.sign() == direction.sign()) return;
        breakpoint_event event;
        event.root.numerator.set_abs(base);
        event.root.denominator.set_abs(direction);
        event.solution_entry = solution_entry;
        event.direction_sign = direction.sign();
        breakpoint_events_.push_back(std::move(event));
    }

    void sweep_direction(size_t direction)
    {
        size_t interval_lower_size = 0;
        size_t interval_positive_size = 0;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            const int base_sign = solution_(row, 0).sign();
            const int direction_sign = directions_(row, direction).sign();
            interval_lower_size += base_sign != 0 || direction_sign != 0;
            interval_positive_size += base_sign > 0 || (base_sign == 0 && direction_sign > 0);
        }

        size_t interval_upper_size = 0;
        size_t interval_gain_size = 0;
        size_t interval_loss_size = 0;
        for (size_t row = 0; row < product_.size(); ++row) {
            const int base_sign = product_[row].sign();
            const int direction_sign = direction_products_(row, direction).sign();
            interval_upper_size += base_sign > 0 || (base_sign == 0 && direction_sign >= 0);
            interval_loss_size += base_sign == 0 && direction_sign < 0;
        }

        positive_ratio sample;
        sample.numerator = breakpoint_events_.front().root.numerator;
        sample.denominator = breakpoint_events_.front().root.denominator;
        sample.denominator.multiply(2);
        consider_signature(
            interval_lower_size, interval_upper_size, interval_positive_size, interval_gain_size, interval_loss_size, sample);

        size_t group_begin = 0;
        while (group_begin < breakpoint_events_.size() && !negative_witness_found_) {
            size_t group_end = group_begin + 1;
            while (group_end < breakpoint_events_.size()
                   && ratio_equal(breakpoint_events_[group_begin].root, breakpoint_events_[group_end].root))
                ++group_end;

            size_t root_lower_size = interval_lower_size;
            size_t root_upper_size = interval_upper_size;
            size_t root_positive_size = interval_positive_size;
            size_t solution_event_count = 0;
            size_t positive_solution_event_count = 0;
            size_t positive_product_event_count = 0;
            size_t negative_product_event_count = 0;
            for (size_t index = group_begin; index < group_end; ++index) {
                const breakpoint_event& event = breakpoint_events_[index];
                if (event.solution_entry) {
                    --root_lower_size;
                    ++solution_event_count;
                    if (event.direction_sign < 0) --root_positive_size;
                    else ++positive_solution_event_count;
                } else if (event.direction_sign > 0) {
                    ++root_upper_size;
                    ++positive_product_event_count;
                } else {
                    ++negative_product_event_count;
                }
            }

            const size_t root_gain_size = interval_gain_size + positive_product_event_count;
            const size_t root_loss_size = interval_loss_size;
            consider_signature(root_lower_size, root_upper_size, root_positive_size, root_gain_size, root_loss_size,
                               breakpoint_events_[group_begin].root);
            if (negative_witness_found_) return;
            interval_lower_size = root_lower_size + solution_event_count;
            interval_upper_size = root_upper_size - negative_product_event_count;
            interval_positive_size = root_positive_size + positive_solution_event_count;
            interval_gain_size = root_gain_size;
            interval_loss_size = root_loss_size + negative_product_event_count;

            if (group_end < breakpoint_events_.size())
                midpoint(sample, breakpoint_events_[group_begin].root, breakpoint_events_[group_end].root);
            else {
                sample.numerator = breakpoint_events_[group_begin].root.numerator;
                sample.numerator += breakpoint_events_[group_begin].root.denominator;
                sample.denominator = breakpoint_events_[group_begin].root.denominator;
            }
            consider_signature(
                interval_lower_size, interval_upper_size, interval_positive_size, interval_gain_size, interval_loss_size, sample);
            group_begin = group_end;
        }
    }

    static void midpoint(positive_ratio& result, const positive_ratio& left, const positive_ratio& right)
    {
        integer second_term;
        result.numerator.set_product(left.numerator, right.denominator);
        second_term.set_product(right.numerator, left.denominator);
        result.numerator += second_term;
        result.denominator.set_product(left.denominator, right.denominator);
        result.denominator.multiply(2);
    }

    void consider_signature(size_t lower_size, size_t upper_size, size_t positive_size, size_t gains, size_t losses,
                            const positive_ratio& candidate)
    {
        if (positive_size == 0) {
            negative_witness_found_ = true;
            return;
        }
        assert(upper_size >= lower_size);
        const coverage_score candidate_score{upper_size - lower_size, upper_size};
        if (better_ray_candidate(candidate_score, gains, losses, ray_best_initialized_, ray_best_score_, ray_best_gains_,
                                 ray_best_losses_)) {
            ray_best_score_ = candidate_score;
            ray_best_numerator_ = candidate.numerator;
            ray_best_denominator_ = candidate.denominator;
            ray_best_gains_ = gains;
            ray_best_losses_ = losses;
            ray_best_initialized_ = true;
        }
        if (better_score(candidate_score, best_score_)) {
            best_score_ = candidate_score;
            best_numerator_ = candidate.numerator;
            best_denominator_ = candidate.denominator;
        }
    }

    void retain_ray_candidate(size_t direction)
    {
        ray_candidate candidate;
        candidate.step.numerator = ray_best_numerator_;
        candidate.step.denominator = ray_best_denominator_;
        candidate.score = ray_best_score_;
        candidate.direction = direction;
        candidate.gains = ray_best_gains_;
        candidate.losses = ray_best_losses_;

        const auto position = std::find_if(ray_shortlist_.begin(), ray_shortlist_.end(), [&](const ray_candidate& current) {
            return better_shortlist_candidate(candidate, current);
        });
        if (ray_shortlist_.size() == shortlist_limit_ && position == ray_shortlist_.end()) return;
        ray_shortlist_.insert(position, std::move(candidate));
        if (ray_shortlist_.size() > shortlist_limit_) ray_shortlist_.pop_back();
    }

    void materialize_upper(size_t candidate_index)
    {
        const ray_candidate& candidate = ray_shortlist_[candidate_index];
        support& upper = shortlist_uppers_[candidate_index];
        upper.clear();
        size_t gains = 0;
        size_t losses = 0;
        for (size_t row = 0; row < product_.size(); ++row) {
            timeout_checkpoint();
            set_linear_combination(scratch_, product_[row], direction_products_(row, candidate.direction),
                                   candidate.step.numerator, candidate.step.denominator);
            const bool current_upper = product_[row].sign() >= 0;
            const bool candidate_upper = scratch_.sign() >= 0;
            if (candidate_upper) upper.set(row);
            gains += !current_upper && candidate_upper;
            losses += current_upper && !candidate_upper;
        }
        assert(gains == candidate.gains);
        assert(losses == candidate.losses);
    }

    pair_score score_pair(size_t first, size_t second) const
    {
        timeout_checkpoint();
        pair_score result;
        for (size_t row = 0; row < product_.size(); ++row) {
            const bool base_upper = product_[row].sign() >= 0;
            const bool first_upper = shortlist_uppers_[first].contains(row);
            const bool second_upper = shortlist_uppers_[second].contains(row);
            result.union_gains += !base_upper && (first_upper || second_upper);
            result.common_losses += base_upper && !first_upper && !second_upper;
            result.total_gains += !base_upper && first_upper;
            result.total_gains += !base_upper && second_upper;
            result.union_losses += base_upper && (!first_upper || !second_upper);
        }
        return result;
    }

    std::array<ray_pair, 2> select_pairs() const
    {
        std::array<ray_pair, 2> selected;
        for (size_t first = 0; first < ray_shortlist_.size(); ++first) {
            for (size_t second = first + 1; second < ray_shortlist_.size(); ++second) {
                ray_pair candidate{first, second, score_pair(first, second), true};
                if (better_pair(candidate, selected[0])) {
                    selected[1] = selected[0];
                    selected[0] = candidate;
                } else if (better_pair(candidate, selected[1])) {
                    selected[1] = candidate;
                }
            }
        }
        return selected;
    }

    void build_combined_direction(const ray_pair& pair, size_t target_column)
    {
        const ray_candidate& first = ray_shortlist_[pair.first];
        const ray_candidate& second = ray_shortlist_[pair.second];
        integer first_coefficient;
        integer second_coefficient;
        first_coefficient.set_product(first.step.numerator, second.step.denominator);
        second_coefficient.set_product(second.step.numerator, first.step.denominator);

        integer content;
        fmpz_gcd(content.native_handle(), first_coefficient.native_handle(), second_coefficient.native_handle());
        if (!content.is_one()) {
            first_coefficient.divide_exact(content);
            second_coefficient.divide_exact(content);
        }

        for (size_t row = 0; row < directions_.rows(); ++row) {
            scratch_.set_product(directions_(row, first.direction), first_coefficient);
            scratch_.addmul(directions_(row, second.direction), second_coefficient);
            combined_directions_(row, target_column) = scratch_;
        }
        for (size_t row = 0; row < direction_products_.rows(); ++row) {
            scratch_.set_product(direction_products_(row, first.direction), first_coefficient);
            scratch_.addmul(direction_products_(row, second.direction), second_coefficient);
            combined_products_(row, target_column) = scratch_;
        }
    }

    void install_combined_direction(size_t source_column)
    {
        for (size_t row = 0; row < directions_.rows(); ++row) directions_(row, 0) = combined_directions_(row, source_column);
        for (size_t row = 0; row < direction_products_.rows(); ++row)
            direction_products_(row, 0) = combined_products_(row, source_column);
    }

    bool try_combined_rays()
    {
        if (ray_shortlist_.size() < 2) return true;
        for (size_t candidate = 0; candidate < ray_shortlist_.size(); ++candidate) materialize_upper(candidate);

        const std::array<ray_pair, 2> pairs = select_pairs();
        if (!pairs[0].initialized) return true;
        const size_t ray_count = pairs[1].initialized ? 2 : 1;
        combined_directions_.resize(directions_.rows(), ray_count);
        combined_products_.resize(direction_products_.rows(), ray_count);
        for (size_t ray = 0; ray < ray_count; ++ray) build_combined_direction(pairs[ray], ray);

        coverage_score selected_score = current_score_;
        integer selected_numerator;
        integer selected_denominator;
        size_t selected_ray = 0;
        bool selected = false;
        for (size_t ray = 0; ray < ray_count; ++ray) {
            install_combined_direction(ray);
#ifdef COPOSIT_XXX_TESTING
            ++combined_ray_sweep_count_;
#endif
            if (!search_direction(0, false)) return false;
            if (!best_numerator_.is_zero() && better_score(best_score_, selected_score)) {
                selected_score = best_score_;
                selected_numerator = best_numerator_;
                selected_denominator = best_denominator_;
                selected_ray = ray;
                selected = true;
            }
            if (selected_score.width + 1 == product_.size()) break;
        }

        if (!selected) return true;
        install_combined_direction(selected_ray);
        apply_candidate(0, selected_numerator, selected_denominator);
        current_score_ = selected_score;
#ifdef COPOSIT_XXX_TESTING
        ++optimized_certificate_count_;
        ++combined_ray_improvement_count_;
#endif
        return true;
    }

    void apply_candidate(size_t direction, const integer& numerator, const integer& denominator)
    {
        for (size_t row = 0; row < solution_.rows(); ++row) {
            set_linear_combination(scratch_, solution_(row, 0), directions_(row, direction), numerator, denominator);
            solution_(row, 0) = scratch_;
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            set_linear_combination(scratch_, product_[row], direction_products_(row, direction), numerator, denominator);
            product_[row] = scratch_;
        }
        remove_common_content();
    }

    void remove_common_content()
    {
        integer content;
        integer next;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            fmpz_gcd(next.native_handle(), content.native_handle(), solution_(row, 0).native_handle());
            content = next;
            if (content.is_one()) return;
        }
        if (content.is_zero() || content.is_one()) return;
        for (size_t row = 0; row < solution_.rows(); ++row) solution_(row, 0).divide_exact(content);
        for (integer& value : product_) value.divide_exact(content);
    }

    static bool all_nonpositive(const matrix_integer& vectors, size_t column)
    {
        bool result = true;
        for (size_t row = 0; row < vectors.rows(); ++row) result &= vectors(row, column).sign() <= 0;
        return result;
    }

    static coverage_score score(const matrix_integer& vectors, size_t vector_column, const std::vector<integer>& products)
    {
        size_t lower_size = 0;
        for (size_t row = 0; row < vectors.rows(); ++row) lower_size += !vectors(row, vector_column).is_zero();
        size_t upper_size = 0;
        for (const integer& value : products) upper_size += value.sign() >= 0;
        assert(upper_size >= lower_size);
        return {upper_size - lower_size, upper_size};
    }

    static void set_linear_combination(integer& result, integer::const_reference base, integer::const_reference direction,
                                       integer::const_reference numerator, integer::const_reference denominator)
    {
        result.set_product(base, denominator);
        result.addmul(direction, numerator);
    }

    void calculate_product(
        const matrix_integer& matrix, const matrix_integer& vectors, size_t vector_column, std::vector<integer>& product)
    {
        for (integer& value : product) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product[row].addmul(matrix(row, indices_[local]), vectors(local, vector_column));
        }
    }

    void calculate_nonsingular_product(const matrix_integer& matrix, const matrix_integer& vectors, size_t vector_column,
                                       const integer& denominator, std::vector<integer>& product)
    {
        size_t local_row = 0;
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            product[row].set_zero();
            if (local_row < indices_.size() && row == indices_[local_row]) {
                product[row] = denominator;
                ++local_row;
                continue;
            }
            for (size_t local = 0; local < indices_.size(); ++local)
                product[row].addmul(matrix(row, indices_[local]), vectors(local, vector_column));
        }
    }

    void calculate_nonsingular_product(const matrix_integer& matrix, const matrix_integer& vectors, size_t vector_column,
                                       const integer& denominator, matrix_integer& products, size_t product_column)
    {
        size_t local_row = 0;
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            products(row, product_column).set_zero();
            if (local_row < indices_.size() && row == indices_[local_row]) {
                if (local_row == vector_column) products(row, product_column) = denominator;
                ++local_row;
                continue;
            }
            for (size_t local = 0; local < indices_.size(); ++local)
                products(row, product_column).addmul(matrix(row, indices_[local]), vectors(local, vector_column));
        }
    }

    bool add_certificate()
    {
        support lower(product_.size());
        support upper(product_.size());
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) lower.set(indices_[local]);
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            if (product_[row].sign() >= 0) upper.set(row);
        }

        bool solution_nonnegative = true;
        integer quadratic;
        for (size_t local = 0; local < indices_.size(); ++local) {
            solution_nonnegative &= solution_(local, 0).sign() >= 0;
            quadratic.addmul(solution_(local, 0), product_[indices_[local]]);
        }
        if (solution_nonnegative && quadratic.is_zero()) {
            if (!record_zero_witness()) return false;
        }

#ifdef COPOSIT_XXX_TESTING
        if (captured_lower_ != nullptr) {
            *captured_lower_ = lower;
            *captured_upper_ = upper;
            return true;
        }
#endif
        offer_interval(lower, upper, true);
        return true;
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    face_result analyze_kkt(const matrix_integer& matrix)
    {
        const size_t cardinality = indices_.size();
        face_result result;
        kkt_solution_.resize(cardinality, 1);
        kkt_products_.resize(dimension_, 1);

        if (cardinality == 1) {
            kkt_denominator_.set_one();
            kkt_solution_(0, 0).set_one();
            kkt_payoff_ = matrix(indices_[0], indices_[0]);
        } else {
            const size_t reduced_dimension = cardinality - 1;
            const size_t reference = indices_.back();
            kkt_reduced_.resize(reduced_dimension, reduced_dimension);
            kkt_rhs_.resize(reduced_dimension, 1);
            for (size_t row = 0; row < reduced_dimension; ++row) {
                const size_t original_row = indices_[row];
                kkt_rhs_(row, 0).set_difference(matrix(reference, reference), matrix(original_row, reference));
                for (size_t column = 0; column <= row; ++column) {
                    const size_t original_column = indices_[column];
                    kkt_reduced_(row, column) = matrix(original_row, original_column);
                    kkt_reduced_(row, column) -= matrix(original_row, reference);
                    kkt_reduced_(row, column) -= matrix(reference, original_column);
                    kkt_reduced_(row, column) += matrix(reference, reference);
                }
            }

            result.nonsingular = kkt_factorization_.factorize_inplace(kkt_reduced_) != 0;
            result.nullity = reduced_dimension - kkt_factorization_.rank();
            result.positive_semidefinite = kkt_factorization_.is_positive_semidefinite();
            if (result.nonsingular) {
                kkt_factorization_.solve_inplace(kkt_rhs_, kkt_denominator_, kkt_reduced_);
            } else {
                kkt_original_rhs_ = kkt_rhs_;
                result.consistent =
                    kkt_factorization_.solve_consistent_inplace(kkt_rhs_, kkt_denominator_, kkt_reduced_);
                if (!result.consistent) return result;
            }

            integer sum;
            for (size_t row = 0; row < reduced_dimension; ++row) {
                kkt_solution_(row, 0) = kkt_rhs_(row, 0);
                sum += kkt_rhs_(row, 0);
            }
            kkt_solution_(cardinality - 1, 0).set_difference(kkt_denominator_, sum);
            kkt_payoff_.set_zero();
            for (size_t position = 0; position < cardinality; ++position)
                kkt_payoff_.addmul(matrix(reference, indices_[position]), kkt_solution_(position, 0));
        }

        result.feasible = true;
        for (size_t position = 0; position < cardinality; ++position)
            result.feasible &= kkt_solution_(position, 0).sign() >= 0;
        if (!result.feasible) return result;

        result.is_kkt = true;
        for (size_t row = 0; row < dimension_; ++row) {
            kkt_products_(row, 0).set_zero();
            for (size_t position = 0; position < cardinality; ++position)
                kkt_products_(row, 0).addmul(matrix(row, indices_[position]), kkt_solution_(position, 0));
            if (kkt_products_(row, 0).compare(kkt_payoff_) < 0) result.is_kkt = false;
        }
        return result;
    }

    bool process_kkt_result(const support& exact, const face_result& face)
    {
        if (!face.consistent) return true;
        record_face_stationary_event(face);
        if (face.feasible && kkt_payoff_.sign() < 0) return false;
        if (face.feasible && kkt_payoff_.is_zero() && !record_zero_witness()) return false;

        if (face.positive_semidefinite && kkt_payoff_.sign() >= 0) {
            support empty(dimension_);
            offer_interval(empty, exact, kkt_payoff_.sign() > 0);
        }
        if (face.is_kkt && kkt_payoff_.sign() >= 0) {
            support full(dimension_);
            for (size_t index = 0; index < dimension_; ++index) full.set(index);
            offer_interval(kkt_positive_support(), full, kkt_payoff_.sign() > 0);
        }
        return true;
    }

    void record_face_stationary_event(const face_result& face) const
    {
        if (!diagnostics_.active() || !face.feasible) return;

        std::ostringstream event;
        event << "event=xxx_two_exact_kkt k=" << indices_.size() << " support=[";
        for (size_t position = 0; position < indices_.size(); ++position) {
            if (position != 0) event << ',';
            event << indices_[position] + 1;
        }
        event << "] positive_support=[";
        bool first = true;
        for (size_t position = 0; position < indices_.size(); ++position) {
            if (kkt_solution_(position, 0).sign() <= 0) continue;
            if (!first) event << ',';
            event << indices_[position] + 1;
            first = false;
        }
        event << "] kkt=" << (face.is_kkt ? "yes" : "no")
              << " psd=" << (face.positive_semidefinite ? "yes" : "no") << " nullity=" << face.nullity
              << " payoff_sign=" << kkt_payoff_.sign();
        diagnostics::record_event(event.str());
    }

    void record_terminal_event(const char* outcome) const
    {
        if (!diagnostics_.active()) return;
        std::ostringstream event;
        event << "event=xxx_two_terminal outcome=" << outcome << " k=" << indices_.size() << " support=[";
        for (size_t position = 0; position < indices_.size(); ++position) {
            if (position != 0) event << ',';
            event << indices_[position] + 1;
        }
        event << ']';
        diagnostics::record_event(event.str());
    }

    std::optional<std::vector<size_t>> nonsingular_successor(const support& exact, const face_result& face)
    {
        std::vector<size_t> candidates;
        for (size_t position = 0; position < indices_.size(); ++position)
            if (kkt_solution_(position, 0).sign() < 0) candidates.push_back(position);
        std::sort(candidates.begin(), candidates.end(), [&](size_t left, size_t right) {
            const int comparison = kkt_solution_(left, 0).compare(kkt_solution_(right, 0));
            return comparison != 0 ? comparison < 0 : indices_[left] < indices_[right];
        });
        for (const size_t removed : candidates) {
            std::vector<size_t> next = indices_;
            next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
            if (admissible(next)) return next;
        }
        if (!candidates.empty()) return std::nullopt;

        std::vector<size_t> nonzero;
        nonzero.reserve(indices_.size());
        std::vector<size_t> zeros;
        for (size_t position = 0; position < indices_.size(); ++position)
            if (!kkt_solution_(position, 0).is_zero())
                nonzero.push_back(indices_[position]);
            else
                zeros.push_back(position);
        if (nonzero.size() != indices_.size()) {
            if (admissible(nonzero)) return nonzero;
            for (const size_t removed : zeros) {
                std::vector<size_t> next = indices_;
                next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
                if (next != nonzero && admissible(next)) return next;
            }
            return std::nullopt;
        }
        if (face.is_kkt) return std::nullopt;

        candidates.clear();
        for (size_t index = 0; index < dimension_; ++index) {
            if (exact.contains(index)) continue;
            if (kkt_products_(index, 0).compare(kkt_payoff_) < 0) candidates.push_back(index);
        }
        std::sort(candidates.begin(), candidates.end(), [&](size_t left, size_t right) {
            const int comparison = kkt_products_(left, 0).compare(kkt_products_(right, 0));
            return comparison != 0 ? comparison < 0 : left < right;
        });
        for (const size_t added : candidates) {
            std::vector<size_t> next = indices_;
            next.insert(std::lower_bound(next.begin(), next.end(), added), added);
            if (admissible(next)) return next;
        }
        return std::nullopt;
    }

    std::optional<std::vector<size_t>> singular_successor(const face_result& face)
    {
        const size_t reduced_dimension = indices_.size() - 1;
        kkt_basis_.resize(reduced_dimension, face.nullity);
        kkt_factorization_.nullspace_basis(kkt_basis_, kkt_reduced_);

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
            for (size_t row = 0; row < reduced_dimension; ++row)
                dot.addmul(kkt_basis_(row, column), kkt_original_rhs_(row, 0));
            if (dot.is_zero()) continue;
            found_separating_direction = true;
            if (auto next = boundary_support(column, dot.sign() > 0 ? 1 : -1)) return next;
        }
        if (found_separating_direction) return std::nullopt;
        throw std::logic_error("an inconsistent symmetric system has no separating nullspace direction");
    }

    std::optional<std::vector<size_t>> boundary_support(size_t column, int orientation)
    {
        kkt_direction_.clear();
        kkt_direction_.resize(indices_.size());
        for (size_t row = 0; row + 1 < indices_.size(); ++row) {
            kkt_direction_[row] = kkt_basis_(row, column);
            if (orientation < 0) kkt_direction_[row].negate();
            kkt_direction_.back() -= kkt_direction_[row];
        }

        std::optional<size_t> minimum;
        for (size_t position = 0; position < kkt_direction_.size(); ++position) {
            if (kkt_direction_[position].sign() >= 0) continue;
            if (!minimum || kkt_direction_[position].compare(kkt_direction_[*minimum]) < 0) minimum = position;
        }
        assert(minimum);
        std::vector<size_t> next;
        next.reserve(indices_.size() - 1);
        for (size_t position = 0; position < indices_.size(); ++position)
            if (kkt_direction_[position].compare(kkt_direction_[*minimum]) != 0) next.push_back(indices_[position]);
        if (admissible(next)) return next;
        return std::nullopt;
    }

    bool admissible(const std::vector<size_t>& indices)
    {
        if (indices.empty()) return false;
        const support candidate = make_support(indices);
        return path_visited_.find(candidate) == path_visited_.end() && supports_->available(indices);
    }

    support make_support(const std::vector<size_t>& indices) const
    {
        support result(dimension_);
        for (const size_t index : indices) result.set(index);
        return result;
    }

    support kkt_positive_support() const
    {
        support result(dimension_);
        for (size_t position = 0; position < indices_.size(); ++position)
            if (kkt_solution_(position, 0).sign() > 0) result.set(indices_[position]);
        assert(!result.empty());
        return result;
    }

    void offer_interval(const support& lower, const support& upper, bool strict_safe)
    {
        path_intervals_.push_back({lower, upper, strict_safe});
    }

    void commit_path_intervals()
    {
#ifdef COPOSIT_XXX_TESTING
        last_committed_path_interval_count_ = path_intervals_.size();
#endif
        for (const buffered_interval& interval : path_intervals_) {
            if (classification_ == nullptr) {
                if (mode_ != copositivity_mode::strictly_copositive || interval.strict_safe)
                    install_interval(interval.lower, interval.upper);
            } else if (classification_->is_strictly_copositive && !interval.strict_safe) {
                pending_.push_back({interval.lower, interval.upper});
            } else {
                install_interval(interval.lower, interval.upper);
            }
        }
        path_intervals_.clear();
    }

    void install_interval(const support& lower, const support& upper)
    {
        supports_->add_interval(lower, upper);
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t index = 0; index < dimension_; ++index) {
            lower_size += lower.contains(index);
            upper_size += upper.contains(index);
        }
        diagnostics_.certificate(upper_size - lower_size, upper_size);
    }

    bool record_zero_witness()
    {
        if (classification_ == nullptr) return mode_ != copositivity_mode::strictly_copositive;
        if (!classification_->is_strictly_copositive) return true;
        classification_->is_strictly_copositive = false;
        for (const pending_interval& interval : pending_) install_interval(interval.lower, interval.upper);
        pending_.clear();
        return true;
    }

#ifdef COPOSIT_XXX_TESTING
    void publish_test_counters() const noexcept
    {
        last_optimized_certificate_count = optimized_certificate_count_;
        last_combined_ray_sweep_count = combined_ray_sweep_count_;
        last_combined_ray_improvement_count = combined_ray_improvement_count_;
    }
#endif

    size_t dimension_;
    fraction_free_ldlt_factorization factorization_;
    fraction_free_ldlt_factorization kkt_factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    matrix_integer direction_products_;
    matrix_integer combined_directions_;
    matrix_integer combined_products_;
    std::vector<integer> product_;
    size_t shortlist_limit_;
    std::vector<size_t> indices_;
    std::vector<breakpoint_event> breakpoint_events_;
    std::vector<ray_candidate> ray_shortlist_;
    std::vector<support> shortlist_uppers_;
    coverage_score current_score_;
    coverage_score best_score_;
    coverage_score ray_best_score_;
    integer best_numerator_;
    integer best_denominator_;
    integer ray_best_numerator_;
    integer ray_best_denominator_;
    integer scratch_;
    matrix_integer kkt_reduced_;
    matrix_integer kkt_rhs_;
    matrix_integer kkt_original_rhs_;
    matrix_integer kkt_solution_;
    matrix_integer kkt_products_;
    matrix_integer kkt_basis_;
    integer kkt_denominator_;
    integer kkt_payoff_;
    std::vector<integer> kkt_direction_;
    double_matrix floating_game_;
    double_matrix floating_reduced_;
    std::vector<double> floating_rhs_;
    std::vector<double> floating_solution_;
    std::vector<double> floating_products_;
    std::vector<int> floating_pivots_;
    double floating_payoff_ = 0.0;
    std::vector<pending_interval> pending_;
    std::vector<buffered_interval> path_intervals_;
    std::set<support> path_visited_;
    support_buckets completed_path_supports_;
    size_t next_path_id_ = 1;
    size_t ray_best_gains_ = 0;
    size_t ray_best_losses_ = 0;
    bool ray_best_initialized_ = false;
    bool negative_witness_found_ = false;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<interval_sat> supports_;
#ifdef COPOSIT_XXX_TESTING
    size_t optimized_certificate_count_ = 0;
    size_t combined_ray_sweep_count_ = 0;
    size_t combined_ray_improvement_count_ = 0;
    support* captured_lower_ = nullptr;
    support* captured_upper_ = nullptr;
    size_t last_committed_path_interval_count_ = 0;
    bool last_path_reached_kkt_ = false;
#endif
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    return dickinson_checker(matrix.rows(), mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    copositivity_classification result{true, true};
    if (!dickinson_checker(matrix.rows(), result).check(matrix)) result = {false, false};
    return result;
}

#ifdef COPOSIT_XXX_TESTING
bool sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(size_t positive_products,
                                                                           size_t negative_products) noexcept
{
    return negative_orientation_has_larger_upper(positive_products, negative_products);
}

size_t sat_halfspace_rays_optimized_certificate_count_for_testing() noexcept
{
    return last_optimized_certificate_count;
}

size_t sat_halfspace_rays_combined_ray_sweep_count_for_testing() noexcept
{
    return last_combined_ray_sweep_count;
}

size_t sat_halfspace_rays_combined_ray_improvement_count_for_testing() noexcept
{
    return last_combined_ray_improvement_count;
}

size_t sat_halfspace_rays_shortlist_limit_for_testing(size_t matrix_dimension, size_t support_dimension)
{
    return ray_shortlist_limit(matrix_dimension, support_dimension);
}

bool sat_halfspace_rays_prefers_ray_candidate_for_testing(size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
                                                          size_t candidate_losses, size_t current_upper, size_t current_width,
                                                          size_t current_gains, size_t current_losses)
{
    return better_ray_candidate({candidate_width, candidate_upper}, candidate_gains, candidate_losses, true,
                                {current_width, current_upper}, current_gains, current_losses);
}

bool sat_halfspace_rays_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::strictly_copositive).check_support_for_testing(matrix, indices);
}

std::array<size_t, 4> xxx_buffered_path_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive).buffered_path_for_testing(matrix, seed);
}

size_t xxx_two_exact_random_paths_for_testing(
    const matrix_integer& matrix, size_t requested, uint64_t random_seed)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .exact_random_paths_for_testing(matrix, requested, random_seed);
}

std::vector<size_t> xxx_two_floating_successor_for_testing(const matrix_integer& matrix, const std::vector<size_t>& current,
                                                           const std::vector<std::vector<size_t>>& known)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .floating_successor_for_testing(matrix, current, known);
}

void xxx_two_completed_path_seed_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed)
{
    dickinson_checker(matrix.rows(), copositivity_mode::copositive).completed_path_seed_for_testing(matrix, seed);
}

bool sat_halfspace_rays_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t sat_halfspace_rays_fixed_support_upper_size_for_testing() noexcept
{
    return last_fixed_support_upper_size;
}

size_t sat_halfspace_rays_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_sat diagram(dimension);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower(dimension);
        support upper(dimension);
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
        }
        diagram.add_interval(lower, upper);
    }

    diagram.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (diagram.take_first(indices)) {
        support exact(dimension);
        for (const size_t index : indices) exact.set(index);
        diagram.add_interval(exact, exact);
        ++count;
    }
    return count;
}

size_t sat_halfspace_rays_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_sat diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.last_interval_clause_size();
}

bool xxx_support_available_for_testing(
    size_t dimension, uint64_t lower_mask, uint64_t upper_mask, uint64_t candidate_mask)
{
    interval_sat diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    std::vector<size_t> candidate;
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
        if ((candidate_mask & (uint64_t{1} << bit)) != 0) candidate.push_back(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.available(candidate);
}
#endif

} // namespace coposit::model
