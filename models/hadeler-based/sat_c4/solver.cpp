#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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

class floating_positive_semidefinite_filter {
public:
    explicit floating_positive_semidefinite_filter(size_t dimension)
        : dimension_(dimension)
    {
    }

    void prepare(const matrix_integer& matrix)
    {
        if (prepared_) return;
        matrix_.assign(dimension_ * dimension_, 0.0);
        slong maximum_exponent = std::numeric_limits<slong>::min();
        for (size_t row = 0; row < dimension_; ++row) {
            for (size_t column = 0; column <= row; ++column) {
                if (matrix(row, column).is_zero()) continue;
                slong exponent = 0;
                static_cast<void>(matrix(row, column).to_dbl_2exp(exponent));
                maximum_exponent = std::max(maximum_exponent, exponent);
            }
        }
        if (maximum_exponent == std::numeric_limits<slong>::min()) {
            prepared_ = true;
            return;
        }

        for (size_t row = 0; row < dimension_; ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) {
                const auto entry = matrix(row, column);
                if (entry.is_zero()) continue;
                slong exponent = 0;
                const double mantissa = entry.to_dbl_2exp(exponent);
                const long double shift = static_cast<long double>(exponent) - static_cast<long double>(maximum_exponent);
                const double value = shift < static_cast<long double>(std::numeric_limits<int>::min())
                    ? 0.0
                    : std::scalbn(mantissa, static_cast<int>(shift));
                matrix_[row * dimension_ + column] = value;
                matrix_[column * dimension_ + row] = value;
            }
        }
        prepared_ = true;
    }

    bool looks_positive_semidefinite(const std::vector<size_t>& indices)
    {
        const size_t order = indices.size();
        principal_.resize(order * order);
        diagonal_.resize(order);
        double scale = 0.0;
        for (size_t row = 0; row < order; ++row) {
            for (size_t column = 0; column <= row; ++column) {
                const double entry = matrix_[indices[row] * dimension_ + indices[column]];
                principal_[row * order + column] = entry;
                scale = std::max(scale, std::abs(entry));
            }
        }
        if (scale == 0.0 || !std::isfinite(scale)) return false;

        const double tolerance = 64.0 * std::numeric_limits<double>::epsilon() * scale * static_cast<double>(order);
        for (size_t column = 0; column < order; ++column) {
            timeout_checkpoint();
            double pivot = principal_[column * order + column];
            for (size_t previous = 0; previous < column; ++previous) {
                const double multiplier = principal_[column * order + previous];
                pivot -= multiplier * multiplier * diagonal_[previous];
            }
            if (!std::isfinite(pivot) || pivot < -tolerance) return false;
            if (pivot <= tolerance) {
                diagonal_[column] = 0.0;
                for (size_t row = column + 1; row < order; ++row) {
                    double entry = principal_[row * order + column];
                    for (size_t previous = 0; previous < column; ++previous) {
                        entry -= principal_[row * order + previous] * principal_[column * order + previous]
                            * diagonal_[previous];
                    }
                    if (!std::isfinite(entry) || std::abs(entry) > tolerance) return false;
                    principal_[row * order + column] = 0.0;
                }
                continue;
            }
            diagonal_[column] = pivot;

            for (size_t row = column + 1; row < order; ++row) {
                double entry = principal_[row * order + column];
                for (size_t previous = 0; previous < column; ++previous) {
                    entry -= principal_[row * order + previous] * principal_[column * order + previous]
                        * diagonal_[previous];
                }
                entry /= pivot;
                if (!std::isfinite(entry)) return false;
                principal_[row * order + column] = entry;
            }
        }
        return true;
    }

    bool looks_reduced_hessian_positive_definite(const std::vector<size_t>& indices)
    {
        if (indices.size() < 2) return true;

        const size_t order = indices.size() - 1;
        const size_t anchor = indices.back();
        principal_.resize(order * order);
        diagonal_.resize(order);
        double scale = 0.0;
        for (size_t row = 0; row < order; ++row) {
            for (size_t column = 0; column <= row; ++column) {
                const double entry = matrix_[indices[row] * dimension_ + indices[column]]
                    - matrix_[indices[row] * dimension_ + anchor]
                    - matrix_[anchor * dimension_ + indices[column]]
                    + matrix_[anchor * dimension_ + anchor];
                principal_[row * order + column] = entry;
                scale = std::max(scale, std::abs(entry));
            }
        }
        if (scale == 0.0 || !std::isfinite(scale)) return false;

        const double tolerance = 64.0 * std::numeric_limits<double>::epsilon() * scale * static_cast<double>(order);
        for (size_t column = 0; column < order; ++column) {
            timeout_checkpoint();
            double pivot = principal_[column * order + column];
            for (size_t previous = 0; previous < column; ++previous) {
                const double multiplier = principal_[column * order + previous];
                pivot -= multiplier * multiplier * diagonal_[previous];
            }
            if (!std::isfinite(pivot) || pivot <= tolerance) return false;
            diagonal_[column] = pivot;

            for (size_t row = column + 1; row < order; ++row) {
                double entry = principal_[row * order + column];
                for (size_t previous = 0; previous < column; ++previous) {
                    entry -= principal_[row * order + previous] * principal_[column * order + previous]
                        * diagonal_[previous];
                }
                entry /= pivot;
                if (!std::isfinite(entry)) return false;
                principal_[row * order + column] = entry;
            }
        }
        return true;
    }

private:
    size_t dimension_;
    bool prepared_ = false;
    std::vector<double> matrix_;
    std::vector<double> principal_;
    std::vector<double> diagonal_;
};

#ifdef COPOSIT_SAT_C4_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
size_t last_pair_curvature_exclusion_count = 0;
size_t last_support_curvature_exclusion_count = 0;
std::array<size_t, 3> last_endpoint_curvature_exclusion_counts{};
size_t last_downward_count = 0;
size_t last_exact_count = 0;
size_t last_high_float_rejection_count = 0;
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
        high_frontier_variable_ = new_variable();
    }

    void start_cardinality(size_t cardinality) noexcept { cardinality_ = cardinality; }

    bool take_first(std::vector<size_t>& indices, bool high_frontier = false)
    {
        assert(cardinality_ >= 1 && cardinality_ <= dimension_);
        solver_.assume(cardinality_outputs_[cardinality_ - 1]);
        if (cardinality_ < dimension_) solver_.assume(-cardinality_outputs_[cardinality_]);
        solver_.assume(high_frontier ? high_frontier_variable_ : -high_frontier_variable_);

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

    void add_interval(const support& lower, const support& upper)
    {
        size_t upper_size = 0;
#ifdef COPOSIT_SAT_C4_TESTING
        last_interval_clause_size_ = 0;
#endif
        for (size_t index = 0; index < dimension_; ++index) {
            const bool in_upper = upper.contains(index);
            if (in_upper) ++upper_size;
            if (lower.contains(index)) {
                solver_.add(-variable(index));
#ifdef COPOSIT_SAT_C4_TESTING
                ++last_interval_clause_size_;
#endif
            } else if (!in_upper) {
                solver_.add(variable(index));
#ifdef COPOSIT_SAT_C4_TESTING
                ++last_interval_clause_size_;
#endif
            }
        }
        if (upper_size < dimension_) {
            solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_SAT_C4_TESTING
            ++last_interval_clause_size_;
#endif
        }
        solver_.add(0);
        ++interval_count_;
    }

    void add_pair_upward_closure(size_t first, size_t second)
    {
        solver_.add(-variable(first));
        solver_.add(-variable(second));
        solver_.add(0);
        ++interval_count_;
    }

    void add_upward_closure(const std::vector<size_t>& indices)
    {
        for (const size_t index : indices) solver_.add(-variable(index));
        solver_.add(0);
        ++interval_count_;
    }

    void add_downward_closure(const std::vector<size_t>& indices)
    {
        size_t local = 0;
        for (size_t index = 0; index < dimension_; ++index) {
            if (local < indices.size() && indices[local] == index) ++local;
            else solver_.add(variable(index));
        }
        solver_.add(0);
        globally_empty_ |= indices.size() == dimension_;
        ++interval_count_;
    }

    void add_exact_support(const std::vector<size_t>& indices)
    {
        for (const size_t index : indices) solver_.add(-variable(index));
        if (indices.size() < dimension_) solver_.add(cardinality_outputs_[indices.size()]);
        solver_.add(0);
        ++interval_count_;
    }

    void reject_from_high_frontier(const std::vector<size_t>& indices)
    {
        for (const size_t index : indices) solver_.add(-variable(index));
        if (indices.size() < dimension_) solver_.add(cardinality_outputs_[indices.size()]);
        solver_.add(-high_frontier_variable_);
        solver_.add(0);
    }

    size_t interval_count() const noexcept { return interval_count_; }
    bool globally_empty() const noexcept { return globally_empty_; }
#ifdef COPOSIT_SAT_C4_TESTING
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
    size_t interval_count_ = 0;
    bool globally_empty_ = false;
    int high_frontier_variable_ = 0;
#ifdef COPOSIT_SAT_C4_TESTING
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
        : factorization_(dimension)
        , endpoint_factorization_(dimension)
        , floating_filter_(dimension)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        endpoint_indices_.reserve(dimension);
        for (auto& endpoint : checked_endpoint_indices_) endpoint.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.emplace_back(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , endpoint_factorization_(dimension)
        , floating_filter_(dimension)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        endpoint_indices_.reserve(dimension);
        for (auto& endpoint : checked_endpoint_indices_) endpoint.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.emplace_back(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_SAT_C4_TESTING
        optimized_certificate_count_ = 0;
        combined_ray_sweep_count_ = 0;
        combined_ray_improvement_count_ = 0;
        pair_curvature_exclusion_count_ = 0;
        support_curvature_exclusion_count_ = 0;
        endpoint_curvature_exclusion_counts_.fill(0);
        downward_count_ = 0;
        exact_count_ = 0;
        high_float_rejection_count_ = 0;
#endif
        supports_.emplace(matrix.rows());
        install_pair_curvature_exclusions(matrix);

        size_t low = 1;
        size_t high = matrix.rows();
        while (low <= matrix.rows()) {
            if (!process_low_support(matrix, low, matrix.rows())) return finish(false);
            if (supports_->globally_empty()) return finish(true);
            if (low <= high && !process_high_support(matrix, low, high)) return finish(false);
            if (supports_->globally_empty()) return finish(true);
        }
        return finish(true);
    }

#ifdef COPOSIT_SAT_C4_TESTING
    bool check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        support lower(matrix.rows());
        support upper(matrix.rows());
        const bool result = optimize_support_for_testing(matrix, indices, lower, upper);
        last_fixed_support_upper_size = 0;
        for (size_t index = 0; index < matrix.rows(); ++index) last_fixed_support_upper_size += upper.contains(index);
        return result;
    }

    bool check_endpoint_stages_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        endpoint_curvature_exclusion_counts_.fill(0);
        supports_.emplace(matrix.rows());
        indices_ = indices;
        const bool result = process_subset(matrix);
        publish_test_counters();
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

    bool reduced_hessian_is_positive_definite_for_testing(
        const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        indices_ = indices;
        principal_.resize(indices.size(), indices.size());
        solution_.resize(indices.size(), 1);
        copy_principal(matrix, indices_, principal_);
        if (factorization_.factorize_inplace(principal_) == 0) {
            factorization_.one_nullspace_vector(solution_, principal_);
            return singular_reduced_hessian_is_positive_definite();
        }
        for (size_t row = 0; row < indices.size(); ++row) solution_(row, 0).set_one();
        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        return nonsingular_reduced_hessian_is_positive_definite();
    }
#endif

private:
    enum class pruning_direction { upward, downward, both };
    enum class endpoint_stage : size_t { traditional, halfspace, rays };

    bool process_low_support(const matrix_integer& matrix, size_t& low, size_t high)
    {
        while (low <= high) {
            diagnostics_.stage(low);
            COPOSIT_SAT_C4_DIAGNOSTICS("stage_low", low);
            supports_->start_cardinality(low);
            if (supports_->take_first(indices_))
                return process_selected_support(matrix, low == high ? pruning_direction::both : pruning_direction::upward, true);
            ++low;
        }
        return true;
    }

    bool process_high_support(const matrix_integer& matrix, size_t low, size_t& high)
    {
        while (low <= high) {
            diagnostics_.stage(high);
            COPOSIT_SAT_C4_DIAGNOSTICS("stage_high", high);
            supports_->start_cardinality(high);
            if (supports_->take_first(indices_, true))
                return process_selected_support(matrix, pruning_direction::downward, false);
            --high;
        }
        return true;
    }

    bool process_selected_support(const matrix_integer& matrix, pruning_direction direction, bool from_low_frontier)
    {
        timeout_checkpoint();
        diagnostics_.visit_support();
        diagnostics_.secondary();
        COPOSIT_SAT_C4_DIAGNOSTICS(from_low_frontier ? "process_low" : "process_high", indices_.size());
        return process_subset(matrix, direction, from_low_frontier);
    }

    void install_pair_curvature_exclusions(const matrix_integer& matrix)
    {
        integer curvature;
        integer doubled_off_diagonal;
        for (size_t first = 0; first < matrix.rows(); ++first) {
            timeout_checkpoint();
            for (size_t second = first + 1; second < matrix.rows(); ++second) {
                curvature = matrix(first, first);
                curvature += matrix(second, second);
                doubled_off_diagonal = matrix(first, second);
                doubled_off_diagonal.multiply(2);
                curvature -= doubled_off_diagonal;
                if (curvature.sign() > 0) continue;

                supports_->add_pair_upward_closure(first, second);
                diagnostics_.certificate();
                COPOSIT_SAT_C4_DIAGNOSTICS("pair_upward", 2);
#ifdef COPOSIT_SAT_C4_TESTING
                ++pair_curvature_exclusion_count_;
#endif
            }
        }
    }

    bool process_subset(
        const matrix_integer& matrix, pruning_direction direction = pruning_direction::upward, bool from_low_frontier = true)
    {
        if (!from_low_frontier) {
            floating_filter_.prepare(matrix);
            if (!floating_filter_.looks_positive_semidefinite(indices_)) {
                supports_->reject_from_high_frontier(indices_);
                COPOSIT_SAT_C4_DIAGNOSTICS("high_float_reject", indices_.size());
#ifdef COPOSIT_SAT_C4_TESTING
                ++high_float_rejection_count_;
#endif
                return true;
            }
        }

        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (singular && diagnostics_.active()) diagnostics_.singular_support(dimension - factorization_.rank());
        if (!from_low_frontier) {
            if (singular) return process_curvature_only_singular(direction);
            return process_curvature_only_nonsingular(direction);
        }
        if (direction == pruning_direction::both && singular && add_singular_psd_downward_closure()) return true;
        if (direction == pruning_direction::both && !singular && factorization_.is_positive_definite()) {
            add_downward_closure();
            return true;
        }
        if (singular) return process_singular_subset(matrix);
        return process_nonsingular_subset(matrix);
    }

    bool process_curvature_only_nonsingular(pruning_direction direction)
    {
        if (factorization_.is_positive_definite()) {
            if (direction == pruning_direction::upward) add_exact_support();
            else add_downward_closure();
            return true;
        }
        if (factorization_.negative_inertia() != 1) {
            if (direction == pruning_direction::both) add_upward_closure();
            else add_exact_support();
            return true;
        }

        for (size_t row = 0; row < solution_.rows(); ++row) solution_(row, 0).set_one();
        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);

        integer delta_numerator;
        bool all_nonpositive = true;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            delta_numerator += solution_(row, 0);
            all_nonpositive &= solution_(row, 0).sign() <= 0;
        }
        if (delta_numerator.sign() >= 0) {
            if (direction == pruning_direction::both) add_upward_closure();
            else add_exact_support();
            return true;
        }
        if (all_nonpositive) return false;

        add_exact_support();
        return true;
    }

    bool process_curvature_only_singular(pruning_direction direction)
    {
        if (direction != pruning_direction::upward && add_singular_psd_downward_closure()) return true;

        if (solution_.rows() - factorization_.rank() != 1 || !factorization_.is_positive_semidefinite()) {
            if (direction == pruning_direction::both) add_upward_closure();
            else add_exact_support();
            return true;
        }

        factorization_.one_nullspace_vector(solution_, principal_);
        integer kernel_sum;
        bool all_nonnegative = true;
        bool all_nonpositive = true;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            kernel_sum += solution_(row, 0);
            all_nonnegative &= solution_(row, 0).sign() >= 0;
            all_nonpositive &= solution_(row, 0).sign() <= 0;
        }
        if (kernel_sum.is_zero()) {
            if (direction == pruning_direction::both) add_upward_closure();
            else add_exact_support();
            return true;
        }

        if (all_nonnegative || all_nonpositive) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }
        add_exact_support();
        return true;
    }

    bool add_singular_psd_downward_closure()
    {
        if (!factorization_.is_positive_semidefinite()) return false;
        for (size_t row = 0; row < solution_.rows(); ++row) solution_(row, 0).set_one();
        integer denominator;
        if (!factorization_.solve_consistent_inplace(solution_, denominator, principal_)) return false;
        add_downward_closure();
        return true;
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

        if (!has_negative_entry) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        if (!singular_reduced_hessian_is_positive_definite()) return add_curvature_exclusion();

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
        for (auto& endpoint : checked_endpoint_indices_) endpoint.clear();
        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);
        if (all_nonpositive(solution_, 0)) return false;
        if (!nonsingular_reduced_hessian_is_positive_definite()) return add_curvature_exclusion();

        calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
        current_score_ = score(solution_, 0, product_);
        bool endpoint_curvature_found = try_endpoint_curvature(matrix, endpoint_stage::traditional);

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
            } while (pass_improved && current_score_.width + 1 < matrix.rows());

            if (!endpoint_curvature_found)
                endpoint_curvature_found = try_endpoint_curvature(matrix, endpoint_stage::halfspace);

            if (current_score_.width + 1 < matrix.rows()) {
                if (!try_combined_rays()) return false;
                if (!endpoint_curvature_found)
                    endpoint_curvature_found = try_endpoint_curvature(matrix, endpoint_stage::rays);
            }
        }

        return add_certificate();
    }

    bool try_endpoint_curvature(const matrix_integer& matrix, endpoint_stage stage)
    {
#ifdef COPOSIT_SAT_C4_TESTING
        if (captured_lower_ != nullptr) return false;
#endif

        endpoint_indices_.clear();
        for (size_t index = 0; index < product_.size(); ++index)
            if (product_[index].sign() >= 0) endpoint_indices_.push_back(index);

        const size_t stage_index = static_cast<size_t>(stage);
        if (endpoint_indices_.size() < 2 || endpoint_indices_.size() == matrix.rows() || endpoint_indices_ == indices_) return false;
        for (size_t earlier = 0; earlier < stage_index; ++earlier)
            if (endpoint_indices_ == checked_endpoint_indices_[earlier]) return false;
        checked_endpoint_indices_[stage_index] = endpoint_indices_;

        floating_filter_.prepare(matrix);
        if (floating_filter_.looks_reduced_hessian_positive_definite(endpoint_indices_)) return false;

        const size_t order = endpoint_indices_.size() - 1;
        const size_t anchor = endpoint_indices_.back();
        endpoint_principal_.resize(order, order);
        for (size_t row = 0; row < order; ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) {
                endpoint_principal_(row, column) = matrix(endpoint_indices_[row], endpoint_indices_[column]);
                endpoint_principal_(row, column) -= matrix(endpoint_indices_[row], anchor);
                endpoint_principal_(row, column) -= matrix(anchor, endpoint_indices_[column]);
                endpoint_principal_(row, column) += matrix(anchor, anchor);
            }
        }
        endpoint_factorization_.factorize_inplace(endpoint_principal_);
        if (endpoint_factorization_.is_positive_definite()) return false;

        supports_->add_upward_closure(endpoint_indices_);
        if (diagnostics_.active()) diagnostics_.certificate(matrix.rows() - endpoint_indices_.size(), matrix.rows());
        const char* label = stage == endpoint_stage::traditional ? "endpoint_traditional_upward"
            : stage == endpoint_stage::halfspace                 ? "endpoint_halfspace_upward"
                                                                 : "endpoint_rays_upward";
        COPOSIT_SAT_C4_DIAGNOSTICS(label, endpoint_indices_.size());
#ifdef COPOSIT_SAT_C4_TESTING
        ++endpoint_curvature_exclusion_counts_[static_cast<size_t>(stage)];
#endif
        return true;
    }

    bool nonsingular_reduced_hessian_is_positive_definite() const
    {
        if (factorization_.is_positive_definite()) return true;
        if (factorization_.negative_inertia() != 1) return false;

        integer delta_numerator;
        for (size_t row = 0; row < solution_.rows(); ++row) delta_numerator += solution_(row, 0);
        return delta_numerator.sign() < 0;
    }

    bool singular_reduced_hessian_is_positive_definite() const
    {
        if (solution_.rows() - factorization_.rank() != 1 || !factorization_.is_positive_semidefinite()) return false;

        integer kernel_sum;
        for (size_t row = 0; row < solution_.rows(); ++row) kernel_sum += solution_(row, 0);
        return !kernel_sum.is_zero();
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
#ifdef COPOSIT_SAT_C4_TESTING
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
#ifdef COPOSIT_SAT_C4_TESTING
            ++combined_ray_sweep_count_;
#endif
            COPOSIT_SAT_C4_DIAGNOSTICS("combined_ray", ray + 1);
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
#ifdef COPOSIT_SAT_C4_TESTING
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
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                lower.set(indices_[local]);
                ++lower_size;
            }
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            if (product_[row].sign() >= 0) {
                upper.set(row);
                ++upper_size;
            }
        }

        bool solution_nonnegative = true;
        integer quadratic;
        for (size_t local = 0; local < indices_.size(); ++local) {
            solution_nonnegative &= solution_(local, 0).sign() >= 0;
            quadratic.addmul(solution_(local, 0), product_[indices_[local]]);
        }
        if (solution_nonnegative && quadratic.is_zero()) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

#ifdef COPOSIT_SAT_C4_TESTING
        if (captured_lower_ != nullptr) {
            *captured_lower_ = lower;
            *captured_upper_ = upper;
            return true;
        }
#endif
        supports_->add_interval(lower, upper);
        if (diagnostics_.active()) diagnostics_.certificate(upper_size - lower_size, upper_size);
        COPOSIT_SAT_C4_DIAGNOSTICS("dickinson", indices_.size());
        return true;
    }

    bool add_curvature_exclusion()
    {
#ifdef COPOSIT_SAT_C4_TESTING
        ++support_curvature_exclusion_count_;
        if (captured_lower_ != nullptr) {
            support lower(product_.size());
            support ceiling(product_.size());
            for (const size_t index : indices_) lower.set(index);
            ceiling.set_all();
            *captured_lower_ = lower;
            *captured_upper_ = ceiling;
            return true;
        }
#endif
        add_upward_closure();
        return true;
    }

    void add_upward_closure()
    {
        supports_->add_upward_closure(indices_);
        if (diagnostics_.active()) diagnostics_.certificate(product_.size() - indices_.size(), product_.size());
        COPOSIT_SAT_C4_DIAGNOSTICS("support_upward", indices_.size());
    }

    void add_downward_closure()
    {
        supports_->add_downward_closure(indices_);
        diagnostics_.certificate();
        COPOSIT_SAT_C4_DIAGNOSTICS("downward", indices_.size());
#ifdef COPOSIT_SAT_C4_TESTING
        ++downward_count_;
#endif
    }

    void add_exact_support()
    {
        supports_->add_exact_support(indices_);
        diagnostics_.certificate();
        COPOSIT_SAT_C4_DIAGNOSTICS("exact", indices_.size());
#ifdef COPOSIT_SAT_C4_TESTING
        ++exact_count_;
#endif
    }

    bool finish(bool result)
    {
        diagnostics_.finish();
#ifdef COPOSIT_SAT_C4_TESTING
        publish_test_counters();
#endif
        return result;
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

#ifdef COPOSIT_SAT_C4_TESTING
    void publish_test_counters() const noexcept
    {
        last_optimized_certificate_count = optimized_certificate_count_;
        last_combined_ray_sweep_count = combined_ray_sweep_count_;
        last_combined_ray_improvement_count = combined_ray_improvement_count_;
        last_pair_curvature_exclusion_count = pair_curvature_exclusion_count_;
        last_support_curvature_exclusion_count = support_curvature_exclusion_count_;
        last_endpoint_curvature_exclusion_counts = endpoint_curvature_exclusion_counts_;
        last_downward_count = downward_count_;
        last_exact_count = exact_count_;
        last_high_float_rejection_count = high_float_rejection_count_;
    }
#endif

    fraction_free_ldlt_factorization factorization_;
    fraction_free_ldlt_factorization endpoint_factorization_;
    floating_positive_semidefinite_filter floating_filter_;
    matrix_integer principal_;
    matrix_integer endpoint_principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    matrix_integer direction_products_;
    matrix_integer combined_directions_;
    matrix_integer combined_products_;
    std::vector<integer> product_;
    size_t shortlist_limit_;
    std::vector<size_t> indices_;
    std::vector<size_t> endpoint_indices_;
    std::array<std::vector<size_t>, 3> checked_endpoint_indices_;
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
    size_t ray_best_gains_ = 0;
    size_t ray_best_losses_ = 0;
    bool ray_best_initialized_ = false;
    bool negative_witness_found_ = false;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<interval_sat> supports_;
#ifdef COPOSIT_SAT_C4_TESTING
    size_t optimized_certificate_count_ = 0;
    size_t combined_ray_sweep_count_ = 0;
    size_t combined_ray_improvement_count_ = 0;
    size_t pair_curvature_exclusion_count_ = 0;
    size_t support_curvature_exclusion_count_ = 0;
    std::array<size_t, 3> endpoint_curvature_exclusion_counts_{};
    size_t downward_count_ = 0;
    size_t exact_count_ = 0;
    size_t high_float_rejection_count_ = 0;
    support* captured_lower_ = nullptr;
    support* captured_upper_ = nullptr;
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

#ifdef COPOSIT_SAT_C4_TESTING
bool sat_c4_prefers_negative_singular_orientation_for_testing(size_t positive_products,
                                                               size_t negative_products) noexcept
{
    return negative_orientation_has_larger_upper(positive_products, negative_products);
}

size_t sat_c4_optimized_certificate_count_for_testing() noexcept
{
    return last_optimized_certificate_count;
}

size_t sat_c4_combined_ray_sweep_count_for_testing() noexcept
{
    return last_combined_ray_sweep_count;
}

size_t sat_c4_combined_ray_improvement_count_for_testing() noexcept
{
    return last_combined_ray_improvement_count;
}

size_t sat_c4_pair_curvature_exclusion_count_for_testing() noexcept
{
    return last_pair_curvature_exclusion_count;
}

size_t sat_c4_support_curvature_exclusion_count_for_testing() noexcept
{
    return last_support_curvature_exclusion_count;
}

size_t sat_c4_endpoint_curvature_exclusion_count_for_testing(size_t stage) noexcept
{
    return stage < last_endpoint_curvature_exclusion_counts.size() ? last_endpoint_curvature_exclusion_counts[stage] : 0;
}

size_t sat_c4_pair_upward_count_for_testing() noexcept
{
    return last_pair_curvature_exclusion_count;
}

size_t sat_c4_support_upward_count_for_testing() noexcept
{
    return last_support_curvature_exclusion_count;
}

size_t sat_c4_downward_count_for_testing() noexcept
{
    return last_downward_count;
}

size_t sat_c4_exact_count_for_testing() noexcept
{
    return last_exact_count;
}

size_t sat_c4_high_float_rejection_count_for_testing() noexcept
{
    return last_high_float_rejection_count;
}

bool sat_c4_floating_psd_candidate_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    floating_positive_semidefinite_filter filter(matrix.rows());
    filter.prepare(matrix);
    return filter.looks_positive_semidefinite(indices);
}

bool sat_c4_reduced_hessian_is_positive_definite_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .reduced_hessian_is_positive_definite_for_testing(matrix, indices);
}

size_t sat_c4_shortlist_limit_for_testing(size_t matrix_dimension, size_t support_dimension)
{
    return ray_shortlist_limit(matrix_dimension, support_dimension);
}

bool sat_c4_prefers_ray_candidate_for_testing(size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
                                               size_t candidate_losses, size_t current_upper, size_t current_width,
                                               size_t current_gains, size_t current_losses)
{
    return better_ray_candidate({candidate_width, candidate_upper}, candidate_gains, candidate_losses, true,
                                {current_width, current_upper}, current_gains, current_losses);
}

bool sat_c4_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::strictly_copositive).check_support_for_testing(matrix, indices);
}

bool sat_c4_check_endpoint_stages_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .check_endpoint_stages_for_testing(matrix, indices);
}

bool sat_c4_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t sat_c4_fixed_support_upper_size_for_testing() noexcept
{
    return last_fixed_support_upper_size;
}

size_t sat_c4_uncovered_count(
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

size_t sat_c4_pair_exclusion_uncovered_count(
    size_t dimension, size_t cardinality, size_t first, size_t second)
{
    interval_sat diagram(dimension);
    diagram.add_pair_upward_closure(first, second);
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

size_t sat_c4_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::vector<size_t>>& upward,
    const std::vector<std::vector<size_t>>& downward, const std::vector<std::vector<size_t>>& exact)
{
    interval_sat supports(dimension);
    for (const auto& indices : upward) supports.add_upward_closure(indices);
    for (const auto& indices : downward) supports.add_downward_closure(indices);
    for (const auto& indices : exact) supports.add_exact_support(indices);

    supports.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (supports.take_first(indices)) {
        supports.add_exact_support(indices);
        ++count;
    }
    return count;
}

size_t sat_c4_high_filter_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::vector<size_t>>& rejected, bool high_frontier)
{
    interval_sat supports(dimension);
    for (const auto& indices : rejected) supports.reject_from_high_frontier(indices);

    supports.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (supports.take_first(indices, high_frontier)) {
        supports.add_exact_support(indices);
        ++count;
    }
    return count;
}

size_t sat_c4_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
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
#endif

} // namespace coposit::model
