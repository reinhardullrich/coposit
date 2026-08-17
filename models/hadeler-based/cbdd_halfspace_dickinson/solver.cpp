#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <unordered_map>
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

#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
size_t last_optimized_certificate_count = 0;
#endif

class interval_cbdd {
public:
    explicit interval_cbdd(size_t dimension, diagnostics::tracker* diagnostics = nullptr)
        : dimension_(dimension)
        , diagnostics_(diagnostics)
        , expiring_(dimension + 1, empty)
        , current_support_(dimension)
    {
        nodes_.push_back({dimension_, dimension_, 0, 0}); // Constant false.
        nodes_.push_back({dimension_, dimension_, 1, 1}); // Constant true.
    }

    void start_cardinality(size_t cardinality)
    {
        if (!diagnostics_) {
            expire_before(cardinality);
            remaining_ = subtract(cardinality_family(cardinality), covered_);
            return;
        }
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::cardinality_build);
        expire_before(cardinality);
        remaining_ = subtract(cardinality_family(cardinality), covered_);
        publish_work();
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::support_solve);
    }

    bool take_first(std::vector<size_t>& indices)
    {
        if (remaining_ == empty) return false;

        current_support_.clear();
        size_t root = remaining_;
        while (root != unit) {
            assert(root != empty);
            const node value = nodes_[root];
            if (value.low != empty) {
                root = value.low;
            } else {
                current_support_.set(actual_index(value.bottom));
                root = value.high;
            }
        }
        current_support_.copy_indices_to(indices);
        return true;
    }

    void add_interval(const support& lower, const support& upper, size_t upper_cardinality)
    {
        assert(upper_cardinality >= expiration_cursor_);
        const size_t certificate = interval_family(lower, upper);
        if (!diagnostics_) {
            covered_ = unite(covered_, certificate);
            retain_until(upper_cardinality, certificate);
            remaining_ = subtract(remaining_, certificate);
            return;
        }
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::certificate_union);
        covered_ = unite(covered_, certificate);
        retain_until(upper_cardinality, certificate);
        publish_work();
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::certificate_subtract);
        remaining_ = subtract(remaining_, certificate);
        publish_work();
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::support_solve);
    }

    size_t node_count() const noexcept { return nodes_.size(); }

#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
    size_t expired_bucket_count() const noexcept { return expired_bucket_count_; }
#endif

    size_t maximum_chain_length() const noexcept
    {
        size_t result = 0;
        for (size_t root = 2; root < nodes_.size(); ++root)
            result = std::max(result, nodes_[root].bottom - nodes_[root].top + 1);
        return result;
    }

private:
    static constexpr size_t empty = 0;
    static constexpr size_t unit = 1;

    struct node {
        size_t top;
        size_t bottom;
        size_t low;
        size_t high;
    };

    struct node_key {
        size_t top;
        size_t bottom;
        size_t low;
        size_t high;

        bool operator==(const node_key& other) const noexcept
        {
            return top == other.top && bottom == other.bottom && low == other.low && high == other.high;
        }
    };

    struct node_key_hash {
        size_t operator()(const node_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.top);
            result ^= std::hash<size_t>{}(value.bottom) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            result ^= std::hash<size_t>{}(value.low) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            result ^= std::hash<size_t>{}(value.high) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            return result;
        }
    };

    struct pair_key {
        size_t left;
        size_t right;

        bool operator==(const pair_key& other) const noexcept
        {
            return left == other.left && right == other.right;
        }
    };

    struct pair_key_hash {
        size_t operator()(const pair_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.left);
            return result ^ (std::hash<size_t>{}(value.right) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2));
        }
    };

    size_t actual_index(size_t variable) const noexcept
    {
        return dimension_ - 1 - variable;
    }

    size_t top(size_t root) const noexcept
    {
        return root < 2 ? dimension_ : nodes_[root].top;
    }

    template <bool ReportDiagnostics>
    void operation_checkpoint()
    {
        if constexpr (ReportDiagnostics) {
            ++operations_;
            if (operations_ % diagnostics::decision_diagram_publish_interval == 0) publish_work();
            if ((operations_ & 4095U) == 0) timeout_checkpoint();
        } else {
            if ((++operations_ & 4095U) == 0) timeout_checkpoint();
        }
    }

    void publish_work() noexcept
    {
        if (diagnostics_) diagnostics_->decision_diagram_work(nodes_.size(), operations_);
    }

    size_t make_node(size_t top_value, size_t bottom_value, size_t low, size_t high)
    {
        if (low == high) return low;

        if (low >= 2) {
            const node child = nodes_[low];
            if (child.top == bottom_value + 1 && child.high == high)
                return make_node(top_value, child.bottom, child.low, high);
        }

        const node_key key{top_value, bottom_value, low, high};
        const auto found = unique_.find(key);
        if (found != unique_.end()) return found->second;

        const size_t result = nodes_.size();
        nodes_.push_back({top_value, bottom_value, low, high});
        unique_.emplace(key, result);
        return result;
    }

    size_t make_node(size_t variable_value, size_t low, size_t high)
    {
        return make_node(variable_value, variable_value, low, high);
    }

    size_t interval_family(const support& lower, const support& upper)
    {
        size_t root = unit;
        for (size_t variable_value = dimension_; variable_value-- > 0;) {
            const size_t bit = actual_index(variable_value);
            if (lower.contains(bit)) {
                root = make_node(variable_value, empty, root);
            } else if (!upper.contains(bit)) {
                root = make_node(variable_value, root, empty);
            }
        }
        return root;
    }

    size_t cardinality_family(size_t cardinality)
    {
        const size_t missing = std::numeric_limits<size_t>::max();
        std::vector<std::vector<size_t>> memo(dimension_ + 1, std::vector<size_t>(cardinality + 1, missing));
        std::function<size_t(size_t, size_t)> build = [&](size_t variable_value, size_t needed) -> size_t {
            if (needed > dimension_ - variable_value) return empty;
            if (variable_value == dimension_) return needed == 0 ? unit : empty;
            size_t& cached = memo[variable_value][needed];
            if (cached != missing) return cached;

            const size_t low = build(variable_value + 1, needed);
            const size_t high = needed == 0 ? empty : build(variable_value + 1, needed - 1);
            cached = make_node(variable_value, low, high);
            return cached;
        };
        return build(0, cardinality);
    }

    void retain_until(size_t upper_cardinality, size_t certificate)
    {
        if (upper_cardinality < dimension_)
            expiring_[upper_cardinality] = unite(expiring_[upper_cardinality], certificate);
    }

    void expire_before(size_t cardinality)
    {
        assert(cardinality >= expiration_cursor_);
        while (expiration_cursor_ < cardinality) {
            const size_t expired = expiring_[expiration_cursor_];
            if (expired != empty) {
                covered_ = subtract(covered_, expired);
                expiring_[expiration_cursor_] = empty;
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
                ++expired_bucket_count_;
#endif
            }
            ++expiration_cursor_;
        }
    }

    size_t unite(size_t left, size_t right)
    {
        union_cache_.clear();
        return diagnostics_ ? unite_impl<true>(left, right) : unite_impl<false>(left, right);
    }

    template <bool ReportDiagnostics>
    size_t unite_impl(size_t left, size_t right)
    {
        operation_checkpoint<ReportDiagnostics>();
        if (left == unit || right == unit) return unit;
        if (left == empty || left == right) return right;
        if (right == empty) return left;
        if (right < left) std::swap(left, right);

        const pair_key key{left, right};
        const auto found = union_cache_.find(key);
        if (found != union_cache_.end()) return found->second;

        const size_t top_value = std::min(top(left), top(right));
        const size_t bottom_value = std::min(split_bottom(left, top_value), split_bottom(right, top_value));
        const auto [left_low, left_high] = cofactors(left, bottom_value);
        const auto [right_low, right_high] = cofactors(right, bottom_value);
        const size_t result = make_node(top_value,
                                        bottom_value,
                                        unite_impl<ReportDiagnostics>(left_low, right_low),
                                        unite_impl<ReportDiagnostics>(left_high, right_high));
        union_cache_.emplace(key, result);
        return result;
    }

    size_t subtract(size_t left, size_t right)
    {
        difference_cache_.clear();
        return diagnostics_ ? subtract_impl<true>(left, right) : subtract_impl<false>(left, right);
    }

    template <bool ReportDiagnostics>
    size_t subtract_impl(size_t left, size_t right)
    {
        operation_checkpoint<ReportDiagnostics>();
        if (left == empty || left == right) return empty;
        if (right == empty) return left;
        if (right == unit) return empty;

        const pair_key key{left, right};
        const auto found = difference_cache_.find(key);
        if (found != difference_cache_.end()) return found->second;

        const size_t top_value = std::min(top(left), top(right));
        const size_t bottom_value = std::min(split_bottom(left, top_value), split_bottom(right, top_value));
        const auto [left_low, left_high] = cofactors(left, bottom_value);
        const auto [right_low, right_high] = cofactors(right, bottom_value);
        const size_t result = make_node(top_value,
                                        bottom_value,
                                        subtract_impl<ReportDiagnostics>(left_low, right_low),
                                        subtract_impl<ReportDiagnostics>(left_high, right_high));
        difference_cache_.emplace(key, result);
        return result;
    }

    size_t split_bottom(size_t root, size_t top_value) const noexcept
    {
        if (top(root) == top_value) return nodes_[root].bottom;
        if (root < 2) return dimension_;
        return nodes_[root].top - 1;
    }

    std::pair<size_t, size_t> cofactors(size_t root, size_t bottom_value)
    {
        if (bottom_value < top(root)) return {root, root};

        const node value = nodes_[root];
        if (bottom_value == value.bottom) return {value.low, value.high};
        return {make_node(bottom_value + 1, value.bottom, value.low, value.high), value.high};
    }

    size_t dimension_;
    diagnostics::tracker* diagnostics_;
    std::vector<node> nodes_;
    std::unordered_map<node_key, size_t, node_key_hash> unique_;
    std::unordered_map<pair_key, size_t, pair_key_hash> union_cache_;
    std::unordered_map<pair_key, size_t, pair_key_hash> difference_cache_;
    std::vector<size_t> expiring_;
    size_t covered_ = empty;
    size_t remaining_ = empty;
    size_t expiration_cursor_ = 0;
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
    size_t expired_bucket_count_ = 0;
#endif
    support current_support_;
    uint64_t operations_ = 0;
};

struct coverage_score {
    size_t width = 0;
    size_t upper_size = 0;
};

bool better(const coverage_score& candidate, const coverage_score& current) noexcept
{
    return candidate.width > current.width || (candidate.width == current.width && candidate.upper_size > current.upper_size);
}

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::decision_diagram, dimension)
        , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::decision_diagram, dimension)
        , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
        optimized_certificate_count_ = 0;
#endif
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.decision_diagram_cardinality(subset_dimension, diagnostics::decision_diagram_phase::cardinality_build);
            supports_.start_cardinality(subset_dimension);
            while (supports_.take_first(indices_)) {
                timeout_checkpoint();
                diagnostics_.decision_diagram_support();
                COPOSIT_CBDD_HALFSPACE_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
                    last_optimized_certificate_count = optimized_certificate_count_;
#endif
                    return false;
                }
            }
        }

        diagnostics_.finish();
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
        last_optimized_certificate_count = optimized_certificate_count_;
#endif
        return true;
    }

private:
    bool process_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
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
                for (size_t direction = 0; direction < dimension; ++direction) {
                    if (first_pass)
                        calculate_nonsingular_product(
                            matrix, directions_, direction, direction_denominator, direction_products_, direction);
                    bool improved = false;
                    if (!optimize_direction(direction, improved)) return false;
                    pass_improved |= improved;
                    if (current_score_.width + 1 == matrix.rows()) break;
                }
                first_pass = false;
            } while (pass_improved && current_score_.width + 1 < matrix.rows());
        }

        return add_certificate();
    }

    bool optimize_direction(size_t direction, bool& improved)
    {
        improved = false;
        best_score_ = current_score_;
        best_numerator_.set_zero();
        best_denominator_.set_one();
        negative_witness_found_ = false;

        find_breakpoints(direction);
        if (breakpoint_events_.empty()) return true;
        sweep_direction(direction);
        if (negative_witness_found_) return false;
        if (best_numerator_.is_zero()) return true;

        apply_candidate(direction, best_numerator_, best_denominator_);
        current_score_ = best_score_;
        improved = true;
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
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
        for (size_t row = 0; row < product_.size(); ++row) {
            const int base_sign = product_[row].sign();
            const int direction_sign = direction_products_(row, direction).sign();
            interval_upper_size += base_sign > 0 || (base_sign == 0 && direction_sign >= 0);
        }

        positive_ratio sample;
        sample.numerator = breakpoint_events_.front().root.numerator;
        sample.denominator = breakpoint_events_.front().root.denominator;
        sample.denominator.multiply(2);
        consider_signature(interval_lower_size, interval_upper_size, interval_positive_size, sample);

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
                } else {
                    ++negative_product_event_count;
                }
            }

            consider_signature(
                root_lower_size, root_upper_size, root_positive_size, breakpoint_events_[group_begin].root);
            if (negative_witness_found_) return;
            interval_lower_size = root_lower_size + solution_event_count;
            interval_upper_size = root_upper_size - negative_product_event_count;
            interval_positive_size = root_positive_size + positive_solution_event_count;

            if (group_end < breakpoint_events_.size())
                midpoint(sample, breakpoint_events_[group_begin].root, breakpoint_events_[group_end].root);
            else {
                sample.numerator = breakpoint_events_[group_begin].root.numerator;
                sample.numerator += breakpoint_events_[group_begin].root.denominator;
                sample.denominator = breakpoint_events_[group_begin].root.denominator;
            }
            consider_signature(interval_lower_size, interval_upper_size, interval_positive_size, sample);
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

    void consider_signature(size_t lower_size, size_t upper_size, size_t positive_size, const positive_ratio& candidate)
    {
        if (positive_size == 0) {
            negative_witness_found_ = true;
            return;
        }
        assert(upper_size >= lower_size);
        const coverage_score candidate_score{upper_size - lower_size, upper_size};
        if (!better(candidate_score, best_score_)) return;
        best_score_ = candidate_score;
        best_numerator_ = candidate.numerator;
        best_denominator_ = candidate.denominator;
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

        supports_.add_interval(lower, upper, upper_size);
        if (diagnostics_.active()) diagnostics_.decision_diagram_certificate(upper_size - lower_size, upper_size);
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

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    matrix_integer direction_products_;
    std::vector<integer> product_;
    std::vector<size_t> indices_;
    std::vector<breakpoint_event> breakpoint_events_;
    coverage_score current_score_;
    coverage_score best_score_;
    integer best_numerator_;
    integer best_denominator_;
    integer scratch_;
    bool negative_witness_found_ = false;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    interval_cbdd supports_;
#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
    size_t optimized_certificate_count_ = 0;
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

#ifdef COPOSIT_CBDD_HALFSPACE_DICKINSON_TESTING
bool cbdd_halfspace_prefers_negative_singular_orientation_for_testing(size_t positive_products, size_t negative_products) noexcept
{
    return negative_orientation_has_larger_upper(positive_products, negative_products);
}

size_t cbdd_halfspace_optimized_certificate_count_for_testing() noexcept
{
    return last_optimized_certificate_count;
}

std::pair<size_t, size_t> cbdd_halfspace_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_cbdd diagram(dimension);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower(dimension);
        support upper(dimension);
        size_t upper_size = 0;
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) {
                upper.set(bit);
                ++upper_size;
            }
        }
        diagram.add_interval(lower, upper, upper_size);
    }

    diagram.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (diagram.take_first(indices)) {
        support exact(dimension);
        for (const size_t index : indices) exact.set(index);
        diagram.add_interval(exact, exact, indices.size());
        ++count;
    }
    return {count, diagram.node_count()};
}

size_t cbdd_halfspace_maximum_interval_chain(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_cbdd diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    size_t upper_size = 0;
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) {
            upper.set(bit);
            ++upper_size;
        }
    }
    diagram.add_interval(lower, upper, upper_size);
    return diagram.maximum_chain_length();
}

size_t cbdd_halfspace_expired_bucket_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_cbdd diagram(dimension);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower(dimension);
        support upper(dimension);
        size_t upper_size = 0;
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) {
                upper.set(bit);
                ++upper_size;
            }
        }
        diagram.add_interval(lower, upper, upper_size);
    }
    diagram.start_cardinality(cardinality);
    return diagram.expired_bucket_count();
}
#endif

} // namespace coposit::model
