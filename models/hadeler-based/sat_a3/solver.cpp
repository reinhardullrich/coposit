#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_diagnostics.hpp"
#include "tiny_monotone_extension.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
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

#ifdef COPOSIT_SAT_A3_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
size_t last_monotone_lp_attempt_count = 0;
size_t last_monotone_lp_feasible_count = 0;
size_t last_monotone_extension_count = 0;
size_t last_monotone_exact_rejection_count = 0;
size_t last_monotone_incumbent_upper_size = 0;
size_t last_monotone_final_upper_size = 0;
std::vector<bool> last_monotone_incumbent_upper;
#endif

class timeout_terminator final : public CaDiCaL::Terminator {
public:
    bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
    explicit interval_sat(const support_context& context)
        : context_(context)
        , dimension_(context.dimension())
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

    void add_interval(const support& lower, const support& upper)
    {
        size_t upper_size = 0;
#ifdef COPOSIT_SAT_A3_TESTING
        last_interval_clause_size_ = 0;
#endif
        for (size_t index = 0; index < dimension_; ++index) {
            const bool in_upper = context_.contains(upper, index);
            if (in_upper) ++upper_size;
            if (context_.contains(lower, index)) {
                solver_.add(-variable(index));
#ifdef COPOSIT_SAT_A3_TESTING
                ++last_interval_clause_size_;
#endif
            } else if (!in_upper) {
                solver_.add(variable(index));
#ifdef COPOSIT_SAT_A3_TESTING
                ++last_interval_clause_size_;
#endif
            }
        }
        if (upper_size < dimension_) {
            solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_SAT_A3_TESTING
            ++last_interval_clause_size_;
#endif
        }
        solver_.add(0);
        ++interval_count_;
    }

    size_t interval_count() const noexcept { return interval_count_; }
#ifdef COPOSIT_SAT_A3_TESTING
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

    const support_context& context_;


    size_t dimension_;
    int next_variable_ = 1;
    size_t cardinality_ = 0;
    size_t interval_count_ = 0;
#ifdef COPOSIT_SAT_A3_TESTING
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
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.push_back(support_context_.make());
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.push_back(support_context_.make());
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_SAT_A3_TESTING
        optimized_certificate_count_ = 0;
        combined_ray_sweep_count_ = 0;
        combined_ray_improvement_count_ = 0;
        monotone_lp_attempt_count_ = 0;
        monotone_lp_feasible_count_ = 0;
        monotone_extension_count_ = 0;
        monotone_exact_rejection_count_ = 0;
        monotone_incumbent_upper_size_ = 0;
        monotone_final_upper_size_ = 0;
#endif
        supports_.emplace(support_context_);
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            supports_->start_cardinality(subset_dimension);
            while (supports_->take_first(indices_)) {
                timeout_checkpoint();
                diagnostics_.visit_support();
                diagnostics_.secondary();
                COPOSIT_SAT_A3_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
#ifdef COPOSIT_SAT_A3_TESTING
                    publish_test_counters();
#endif
                    return false;
                }
            }
        }

        diagnostics_.finish();
#ifdef COPOSIT_SAT_A3_TESTING
        publish_test_counters();
#endif
        return true;
    }

#ifdef COPOSIT_SAT_A3_TESTING
    bool check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        support lower = support_context_.make();
        support upper = support_context_.make();
        const bool result = optimize_support_for_testing(matrix, indices, lower, upper);
        last_fixed_support_upper_size = 0;
        for (size_t index = 0; index < matrix.rows(); ++index) last_fixed_support_upper_size += support_context_.contains(upper, index);
        return result;
    }

    bool optimize_support_for_testing(
        const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
    {
        optimized_certificate_count_ = 0;
        combined_ray_sweep_count_ = 0;
        combined_ray_improvement_count_ = 0;
        monotone_lp_attempt_count_ = 0;
        monotone_lp_feasible_count_ = 0;
        monotone_extension_count_ = 0;
        monotone_exact_rejection_count_ = 0;
        monotone_incumbent_upper_size_ = 0;
        monotone_final_upper_size_ = 0;
        indices_ = indices;
        captured_lower_ = &lower;
        captured_upper_ = &upper;
        const bool result = process_subset(matrix);
        captured_lower_ = nullptr;
        captured_upper_ = nullptr;
        publish_test_counters();
        return result;
    }
#endif

private:
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

        if (!has_negative_entry) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

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

            if (current_score_.upper_size < matrix.rows() && !try_monotone_enlargement()) return false;
        }

        return add_certificate();
    }

    enum class exact_lp_candidate_result { unchanged, improved, negative_witness };

    bool try_monotone_enlargement()
    {
        timeout_checkpoint();
        const size_t support_dimension = indices_.size();
        const size_t outside_dimension = product_.size() - support_dimension;
        if (outside_dimension == 0 || !lp_problem_fits(support_dimension, outside_dimension)) return true;

        lp_outside_rows_.clear();
        lp_scaled_rows_.resize(outside_dimension);
        for (auto& row : lp_scaled_rows_) {
            row.resize(support_dimension);
            std::fill(row.begin(), row.end(), 0.0);
        }
        lp_outside_rows_.reserve(outside_dimension);
        size_t outside = 0;
        size_t local_row = 0;
        for (size_t row = 0; row < product_.size(); ++row) {
            if (local_row < indices_.size() && row == indices_[local_row]) {
                ++local_row;
                continue;
            }
            if ((outside & 63U) == 0) timeout_checkpoint();
            lp_outside_rows_.push_back(row);
            slong maximum_exponent = std::numeric_limits<slong>::min();
            for (size_t column = 0; column < support_dimension; ++column) {
                if (direction_products_(row, column).is_zero()) continue;
                slong exponent = 0;
                static_cast<void>(direction_products_(row, column).to_dbl_2exp(exponent));
                maximum_exponent = std::max(maximum_exponent, exponent);
            }
            if (maximum_exponent != std::numeric_limits<slong>::min()) {
                for (size_t column = 0; column < support_dimension; ++column) {
                    if (direction_products_(row, column).is_zero()) continue;
                    slong exponent = 0;
                    const double mantissa = direction_products_(row, column).to_dbl_2exp(exponent);
                    const slong difference = exponent - maximum_exponent;
                    lp_scaled_rows_[outside][column] =
                        difference < -1074 ? 0.0 : std::scalbn(mantissa, static_cast<int>(difference));
                }
            }
            ++outside;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
#ifdef COPOSIT_SAT_A3_TESTING
        monotone_incumbent_upper_size_ = current_score_.upper_size;
        last_monotone_incumbent_upper.assign(product_.size(), false);
        for (size_t row = 0; row < product_.size(); ++row)
            last_monotone_incumbent_upper[row] = product_[row].sign() >= 0;
#endif

        while (current_score_.upper_size < product_.size()) {
            lp_required_rows_.clear();
            for (size_t row = 0; row < outside_dimension; ++row)
                if (product_[lp_outside_rows_[row]].sign() >= 0) lp_required_rows_.push_back(row);

            bool enlarged = false;
            const size_t preserved_count = lp_required_rows_.size();
            for (size_t target = 0; target < outside_dimension; ++target) {
                timeout_checkpoint();
                if (product_[lp_outside_rows_[target]].sign() >= 0) continue;
                lp_required_rows_.resize(preserved_count);
                lp_required_rows_.push_back(target);
                detail::tiny_monotone_extension feasibility(lp_scaled_rows_, lp_required_rows_, deadline);
                const detail::monotone_extension_result proposal = feasibility.solve();
#ifdef COPOSIT_SAT_A3_TESTING
                ++monotone_lp_attempt_count_;
#endif
                if (proposal.interrupted) {
#ifdef COPOSIT_SAT_A3_TESTING
                    monotone_final_upper_size_ = current_score_.upper_size;
#endif
                    return true;
                }
                if (!proposal.feasible) continue;
#ifdef COPOSIT_SAT_A3_TESTING
                ++monotone_lp_feasible_count_;
#endif
                const exact_lp_candidate_result result = consider_lp_point(proposal.point, lp_required_rows_);
                if (result == exact_lp_candidate_result::negative_witness) return false;
                if (result == exact_lp_candidate_result::improved) {
#ifdef COPOSIT_SAT_A3_TESTING
                    ++monotone_extension_count_;
#endif
                    enlarged = true;
                    break;
                }
#ifdef COPOSIT_SAT_A3_TESTING
                ++monotone_exact_rejection_count_;
#endif
            }
            if (!enlarged) break;
        }
#ifdef COPOSIT_SAT_A3_TESTING
        monotone_final_upper_size_ = current_score_.upper_size;
#endif
        return true;
    }

    exact_lp_candidate_result consider_lp_point(const std::vector<double>& point, const std::vector<size_t>& required_outside_rows)
    {
        lp_coefficients_.resize(point.size());
        lp_solution_.resize(point.size(), 1);
        lp_product_.resize(product_.size());
        coverage_score selected_score = current_score_;
        bool selected = false;
        const double maximum = *std::max_element(point.begin(), point.end());
        if (!(maximum > 0.0) || !std::isfinite(maximum)) return exact_lp_candidate_result::unchanged;
        for (const uint64_t scale :
             {uint64_t{1000000}, uint64_t{1000000000}, uint64_t{1000000000000}, uint64_t{1000000000000000}}) {
            bool positive = true;
            for (size_t column = 0; column < point.size(); ++column) {
                const double value = point[column] / maximum;
                if (!(value > 0.0) || !std::isfinite(value)) {
                    positive = false;
                    break;
                }
                const uint64_t coefficient = static_cast<uint64_t>(std::llround(value * static_cast<double>(scale)));
                if (coefficient == 0) {
                    positive = false;
                    break;
                }
                fmpz_set_ui(lp_coefficients_[column].native_handle(), static_cast<ulong>(coefficient));
            }
            if (!positive) continue;

            multiply_directions(directions_, direction_products_, lp_coefficients_, lp_solution_, lp_product_);
            if (all_nonpositive(lp_solution_, 0)) return exact_lp_candidate_result::negative_witness;
            bool preserves_required_rows = true;
            for (const size_t row : required_outside_rows) {
                if (lp_product_[lp_outside_rows_[row]].sign() < 0) {
                    preserves_required_rows = false;
                    break;
                }
            }
            if (!preserves_required_rows) continue;
            const coverage_score candidate_score = score(lp_solution_, 0, lp_product_);
            if (!better_score(candidate_score, selected_score)) continue;
            selected_score = candidate_score;
            selected_solution_ = lp_solution_;
            selected_product_ = lp_product_;
            selected = true;
        }
        if (!selected) return exact_lp_candidate_result::unchanged;
        solution_ = selected_solution_;
        product_ = selected_product_;
        current_score_ = selected_score;
        remove_common_content();
        return exact_lp_candidate_result::improved;
    }

    static void multiply_directions(const matrix_integer& directions, const matrix_integer& products,
                                    const std::vector<integer>& coefficients, matrix_integer& solution,
                                    std::vector<integer>& product)
    {
        for (size_t row = 0; row < directions.rows(); ++row) {
            solution(row, 0).set_zero();
            for (size_t column = 0; column < directions.cols(); ++column)
                solution(row, 0).addmul(directions(row, column), coefficients[column]);
        }
        for (size_t row = 0; row < products.rows(); ++row) {
            product[row].set_zero();
            for (size_t column = 0; column < products.cols(); ++column)
                product[row].addmul(products(row, column), coefficients[column]);
        }
    }

    static bool lp_problem_fits(size_t support_dimension, size_t outside_dimension) noexcept
    {
        constexpr size_t maximum_scaled_entries = size_t{8} * 1024 * 1024;
        return support_dimension == 0 || outside_dimension <= maximum_scaled_entries / support_dimension;
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
#ifdef COPOSIT_SAT_A3_TESTING
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
        support_context_.clear(upper);
        size_t gains = 0;
        size_t losses = 0;
        for (size_t row = 0; row < product_.size(); ++row) {
            timeout_checkpoint();
            set_linear_combination(scratch_, product_[row], direction_products_(row, candidate.direction),
                                   candidate.step.numerator, candidate.step.denominator);
            const bool current_upper = product_[row].sign() >= 0;
            const bool candidate_upper = scratch_.sign() >= 0;
            if (candidate_upper) support_context_.set(upper, row);
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
            const bool first_upper = support_context_.contains(shortlist_uppers_[first], row);
            const bool second_upper = support_context_.contains(shortlist_uppers_[second], row);
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
        original_direction_.resize(directions_.rows());
        original_direction_product_.resize(direction_products_.rows());
        for (size_t row = 0; row < directions_.rows(); ++row) original_direction_[row] = directions_(row, 0);
        for (size_t row = 0; row < direction_products_.rows(); ++row)
            original_direction_product_[row] = direction_products_(row, 0);

        coverage_score selected_score = current_score_;
        integer selected_numerator;
        integer selected_denominator;
        size_t selected_ray = 0;
        bool selected = false;
        for (size_t ray = 0; ray < ray_count; ++ray) {
            install_combined_direction(ray);
#ifdef COPOSIT_SAT_A3_TESTING
            ++combined_ray_sweep_count_;
#endif
            COPOSIT_SAT_A3_DIAGNOSTICS("combined_ray", ray + 1);
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

        if (selected) {
            install_combined_direction(selected_ray);
            apply_candidate(0, selected_numerator, selected_denominator);
            current_score_ = selected_score;
#ifdef COPOSIT_SAT_A3_TESTING
            ++optimized_certificate_count_;
            ++combined_ray_improvement_count_;
#endif
        }
        for (size_t row = 0; row < directions_.rows(); ++row) directions_(row, 0) = original_direction_[row];
        for (size_t row = 0; row < direction_products_.rows(); ++row)
            direction_products_(row, 0) = original_direction_product_[row];
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
        support lower = support_context_.make();
        support upper = support_context_.make();
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                support_context_.set(lower, indices_[local]);
                ++lower_size;
            }
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            if (product_[row].sign() >= 0) {
                support_context_.set(upper, row);
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

#ifdef COPOSIT_SAT_A3_TESTING
        if (captured_lower_ != nullptr) {
            support_context_.copy(*captured_lower_, lower);
            support_context_.copy(*captured_upper_, upper);
            support_context_.release(std::move(lower));
            support_context_.release(std::move(upper));
            return true;
        }
#endif
        supports_->add_interval(lower, upper);
        if (diagnostics_.active()) diagnostics_.certificate(upper_size - lower_size, upper_size);
        support_context_.release(std::move(lower));
        support_context_.release(std::move(upper));
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

#ifdef COPOSIT_SAT_A3_TESTING
    void publish_test_counters() const noexcept
    {
        last_optimized_certificate_count = optimized_certificate_count_;
        last_combined_ray_sweep_count = combined_ray_sweep_count_;
        last_combined_ray_improvement_count = combined_ray_improvement_count_;
        last_monotone_lp_attempt_count = monotone_lp_attempt_count_;
        last_monotone_lp_feasible_count = monotone_lp_feasible_count_;
        last_monotone_extension_count = monotone_extension_count_;
        last_monotone_exact_rejection_count = monotone_exact_rejection_count_;
        last_monotone_incumbent_upper_size = monotone_incumbent_upper_size_;
        last_monotone_final_upper_size = monotone_final_upper_size_;
    }
#endif

    support_context support_context_;

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    matrix_integer direction_products_;
    matrix_integer combined_directions_;
    matrix_integer combined_products_;
    matrix_integer lp_solution_;
    matrix_integer selected_solution_;
    std::vector<integer> product_;
    std::vector<integer> lp_product_;
    std::vector<integer> selected_product_;
    std::vector<integer> lp_coefficients_;
    std::vector<integer> original_direction_;
    std::vector<integer> original_direction_product_;
    std::vector<size_t> lp_outside_rows_;
    std::vector<size_t> lp_required_rows_;
    std::vector<std::vector<double>> lp_scaled_rows_;
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
    size_t ray_best_gains_ = 0;
    size_t ray_best_losses_ = 0;
    bool ray_best_initialized_ = false;
    bool negative_witness_found_ = false;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<interval_sat> supports_;
#ifdef COPOSIT_SAT_A3_TESTING
    size_t optimized_certificate_count_ = 0;
    size_t combined_ray_sweep_count_ = 0;
    size_t combined_ray_improvement_count_ = 0;
    size_t monotone_lp_attempt_count_ = 0;
    size_t monotone_lp_feasible_count_ = 0;
    size_t monotone_extension_count_ = 0;
    size_t monotone_exact_rejection_count_ = 0;
    size_t monotone_incumbent_upper_size_ = 0;
    size_t monotone_final_upper_size_ = 0;
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

#ifdef COPOSIT_SAT_A3_TESTING
bool sat_a3_prefers_negative_singular_orientation_for_testing(size_t positive_products,
                                                                           size_t negative_products) noexcept
{
    return negative_orientation_has_larger_upper(positive_products, negative_products);
}

size_t sat_a3_optimized_certificate_count_for_testing() noexcept
{
    return last_optimized_certificate_count;
}

size_t sat_a3_combined_ray_sweep_count_for_testing() noexcept
{
    return last_combined_ray_sweep_count;
}

size_t sat_a3_combined_ray_improvement_count_for_testing() noexcept
{
    return last_combined_ray_improvement_count;
}

size_t sat_a3_monotone_lp_attempt_count_for_testing() noexcept { return last_monotone_lp_attempt_count; }
size_t sat_a3_monotone_lp_feasible_count_for_testing() noexcept { return last_monotone_lp_feasible_count; }
size_t sat_a3_monotone_extension_count_for_testing() noexcept { return last_monotone_extension_count; }
size_t sat_a3_monotone_exact_rejection_count_for_testing() noexcept { return last_monotone_exact_rejection_count; }
size_t sat_a3_monotone_incumbent_upper_size_for_testing() noexcept { return last_monotone_incumbent_upper_size; }
size_t sat_a3_monotone_final_upper_size_for_testing() noexcept { return last_monotone_final_upper_size; }
bool sat_a3_monotone_incumbent_contains_for_testing(size_t index) noexcept
{
    return index < last_monotone_incumbent_upper.size() && last_monotone_incumbent_upper[index];
}

size_t sat_a3_shortlist_limit_for_testing(size_t matrix_dimension, size_t support_dimension)
{
    return ray_shortlist_limit(matrix_dimension, support_dimension);
}

bool sat_a3_prefers_ray_candidate_for_testing(size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
                                                          size_t candidate_losses, size_t current_upper, size_t current_width,
                                                          size_t current_gains, size_t current_losses)
{
    return better_ray_candidate({candidate_width, candidate_upper}, candidate_gains, candidate_losses, true,
                                {current_width, current_upper}, current_gains, current_losses);
}

bool sat_a3_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::strictly_copositive).check_support_for_testing(matrix, indices);
}

bool sat_a3_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t sat_a3_fixed_support_upper_size_for_testing() noexcept
{
    return last_fixed_support_upper_size;
}

size_t sat_a3_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    support_context support_context_(dimension);
    interval_sat diagram(support_context_);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower = support_context_.make();
        support upper = support_context_.make();
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
        }
        diagram.add_interval(lower, upper);
    }

    diagram.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (diagram.take_first(indices)) {
        support exact = support_context_.make();
        for (const size_t index : indices) support_context_.set(exact, index);
        diagram.add_interval(exact, exact);
        support_context_.release(std::move(exact));
        ++count;
    }
    return count;
}

size_t sat_a3_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    support_context support_context_(dimension);
    interval_sat diagram(support_context_);
    support lower = support_context_.make();
    support upper = support_context_.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.last_interval_clause_size();
}
#endif

} // namespace coposit::model
