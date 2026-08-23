#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/nbc_upward_supports.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
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

#ifdef COPOSIT_NBC_B6_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_pair_curvature_exclusion_count = 0;
#endif

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
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.emplace_back(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , shortlist_limit_(ray_shortlist_limit(dimension, dimension))
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        ray_shortlist_.reserve(shortlist_limit_);
        shortlist_uppers_.reserve(shortlist_limit_);
        for (size_t index = 0; index < shortlist_limit_; ++index) shortlist_uppers_.emplace_back(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_NBC_B6_TESTING
        optimized_certificate_count_ = 0;
        pair_curvature_exclusion_count_ = 0;
#endif
        supports_.emplace(matrix.rows());
        install_pair_curvature_exclusions(matrix);
        supports_->commit_layer(0);

        for (size_t cardinality = 1; cardinality <= matrix.rows(); ++cardinality) {
            if (!process_layer(matrix, cardinality)) return finish(false);
            if (supports_->all_future_covered()) return finish(true);
        }
        return finish(true);
    }

#ifdef COPOSIT_NBC_B6_TESTING
    bool check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        support lower(matrix.rows());
        support upper(matrix.rows());
        return optimize_support_for_testing(matrix, indices, lower, upper);
    }

    bool optimize_support_for_testing(
        const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
    {
        optimized_certificate_count_ = 0;
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
    struct layer_context {
        dickinson_checker& checker;
        const matrix_integer& matrix;
        bool result = true;
    };

    static bool visit_layer_support(void* opaque, const std::vector<size_t>& indices)
    {
        auto& context = *static_cast<layer_context*>(opaque);
        context.checker.indices_ = indices;
        timeout_checkpoint();
        context.checker.diagnostics_.visit_support();
        context.checker.diagnostics_.secondary();
        COPOSIT_NBC_B6_DIAGNOSTICS("process_low", indices.size());
        context.result = context.checker.process_subset(context.matrix);
        return context.result;
    }

    bool process_layer(const matrix_integer& matrix, size_t cardinality)
    {
        diagnostics_.stage(cardinality);
        COPOSIT_NBC_B6_DIAGNOSTICS("stage_low", cardinality);
        layer_context context{*this, matrix};
        const auto result = supports_->enumerate_cardinality(cardinality, &context, &visit_layer_support);
        if (result == nbc_upward_supports::enumeration_result::stopped) return context.result;
        supports_->commit_layer(cardinality);
        return true;
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
                COPOSIT_NBC_B6_DIAGNOSTICS("pair_upward", 2);
#ifdef COPOSIT_NBC_B6_TESTING
                ++pair_curvature_exclusion_count_;
#endif
            }
        }
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
        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);
        if (all_nonpositive(solution_, 0)) return false;
        if (!nonsingular_reduced_hessian_is_positive_definite()) return add_curvature_exclusion();

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
#ifdef COPOSIT_NBC_B6_TESTING
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
            COPOSIT_NBC_B6_DIAGNOSTICS("combined_ray", ray + 1);
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
#ifdef COPOSIT_NBC_B6_TESTING
        ++optimized_certificate_count_;
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

#ifdef COPOSIT_NBC_B6_TESTING
        if (captured_lower_ != nullptr) {
            *captured_lower_ = lower;
            *captured_upper_ = upper;
            return true;
        }
#endif
        supports_->add_interval(lower, upper);
        if (diagnostics_.active()) diagnostics_.certificate(upper_size - lower_size, upper_size);
        COPOSIT_NBC_B6_DIAGNOSTICS("dickinson", indices_.size());
        return true;
    }

    bool add_curvature_exclusion()
    {
#ifdef COPOSIT_NBC_B6_TESTING
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
        COPOSIT_NBC_B6_DIAGNOSTICS("support_upward", indices_.size());
    }

    bool finish(bool result)
    {
        diagnostics_.finish();
#ifdef COPOSIT_NBC_B6_TESTING
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

#ifdef COPOSIT_NBC_B6_TESTING
    void publish_test_counters() const noexcept
    {
        last_optimized_certificate_count = optimized_certificate_count_;
        last_pair_curvature_exclusion_count = pair_curvature_exclusion_count_;
    }
#endif

    fraction_free_ldlt_factorization factorization_;
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
    size_t ray_best_gains_ = 0;
    size_t ray_best_losses_ = 0;
    bool ray_best_initialized_ = false;
    bool negative_witness_found_ = false;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<nbc_upward_supports> supports_;
#ifdef COPOSIT_NBC_B6_TESTING
    size_t optimized_certificate_count_ = 0;
    size_t pair_curvature_exclusion_count_ = 0;
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

#ifdef COPOSIT_NBC_B6_TESTING
size_t nbc_b6_optimized_certificate_count_for_testing() noexcept
{
    return last_optimized_certificate_count;
}

size_t nbc_b6_pair_upward_count_for_testing() noexcept
{
    return last_pair_curvature_exclusion_count;
}

bool nbc_b6_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::strictly_copositive).check_support_for_testing(matrix, indices);
}

bool nbc_b6_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .optimize_support_for_testing(matrix, indices, lower, upper);
}

#endif

} // namespace coposit::model
