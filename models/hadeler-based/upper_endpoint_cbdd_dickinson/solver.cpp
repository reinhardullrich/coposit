#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class interval_cbdd {
public:
    explicit interval_cbdd(size_t dimension, diagnostics::tracker* diagnostics = nullptr)
        : dimension_(dimension)
        , diagnostics_(diagnostics)
        , current_support_(dimension)
    {
        nodes_.push_back({dimension_, dimension_, 0, 0}); // Constant false.
        nodes_.push_back({dimension_, dimension_, 1, 1}); // Constant true.
    }

    void start_cardinality(size_t cardinality)
    {
        if (!diagnostics_) {
            remaining_ = subtract(cardinality_family(cardinality), covered_);
            return;
        }
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::cardinality_build);
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

    void add_interval(const support& lower, const support& upper)
    {
        const size_t certificate = interval_family(lower, upper);
        if (!diagnostics_) {
            covered_ = unite(covered_, certificate);
            remaining_ = subtract(remaining_, certificate);
            return;
        }
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::certificate_union);
        covered_ = unite(covered_, certificate);
        publish_work();
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::certificate_subtract);
        remaining_ = subtract(remaining_, certificate);
        publish_work();
        diagnostics_->decision_diagram_phase_change(diagnostics::decision_diagram_phase::support_solve);
    }

    size_t node_count() const noexcept { return nodes_.size(); }

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
    size_t covered_ = empty;
    size_t remaining_ = empty;
    support current_support_;
    uint64_t operations_ = 0;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::decision_diagram, dimension)
        , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
        , lower_(dimension)
        , upper_(dimension)
        , probe_lower_(dimension)
        , probe_upper_(dimension)
    {
        indices_.reserve(dimension);
        probe_indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::decision_diagram, dimension)
        , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
        , lower_(dimension)
        , upper_(dimension)
        , probe_lower_(dimension)
        , probe_upper_(dimension)
    {
        indices_.reserve(dimension);
        probe_indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.decision_diagram_cardinality(subset_dimension, diagnostics::decision_diagram_phase::cardinality_build);
            supports_.start_cardinality(subset_dimension);
            while (supports_.take_first(indices_)) {
                timeout_checkpoint();
                diagnostics_.decision_diagram_support();
                COPOSIT_UPPER_ENDPOINT_CBDD_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
                    return false;
                }
            }
        }

        diagnostics_.finish();
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
        if (!singular) {
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
        } else {
            factorization_.one_nullspace_vector(solution_, principal_);

            bool has_positive_entry = false;
            for (size_t row = 0; row < dimension; ++row) has_positive_entry |= solution_(row, 0).sign() > 0;
            if (!has_positive_entry) solution_.negate();
        }

        bool all_nonpositive = true;
        bool all_nonnegative = singular;
        for (size_t row = 0; row < dimension; ++row) {
            all_nonpositive &= solution_(row, 0).sign() <= 0;
            all_nonnegative &= solution_(row, 0).sign() >= 0;
        }
        if (all_nonpositive) return false;
        if (all_nonnegative) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        const auto sizes = diagnostics_.active() ? build_certificate<true>(matrix, indices_, solution_, lower_, upper_)
                                                 : build_certificate<false>(matrix, indices_, solution_, lower_, upper_);
        if (!probe_upper_endpoint(matrix, upper_)) return false;
        supports_.add_interval(lower_, upper_);
        if (diagnostics_.active()) diagnostics_.decision_diagram_certificate(sizes.first, sizes.second);
        return true;
    }

    template <bool CountSizes>
    std::pair<size_t, size_t> build_certificate(const matrix_integer& matrix,
                                                const std::vector<size_t>& indices,
                                                const matrix_integer& solution,
                                                support& lower,
                                                support& upper)
    {
        lower.clear();
        upper.clear();
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) {
                lower.set(indices[local]);
                if constexpr (CountSizes) ++lower_size;
            }
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local)
                product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            if (product_[row].sign() >= 0) {
                upper.set(row);
                if constexpr (CountSizes) ++upper_size;
            }
        }

        if constexpr (CountSizes) return {upper_size - lower_size, upper_size};
        return {0, 0};
    }

    bool probe_upper_endpoint(const matrix_integer& matrix, const support& upper)
    {
        upper.copy_indices_to(probe_indices_);
        if (probe_indices_.size() <= indices_.size()) return true;
        if (!probed_upper_endpoints_.insert(upper).second) {
            COPOSIT_UPPER_ENDPOINT_CBDD_DIAGNOSTICS("upper-endpoint-duplicate", probe_indices_.size());
            return true;
        }

        COPOSIT_UPPER_ENDPOINT_CBDD_DIAGNOSTICS("upper-endpoint-probe", probe_indices_.size());
        timeout_checkpoint();
        principal_.resize(probe_indices_.size(), probe_indices_.size());
        solution_.resize(probe_indices_.size(), 1);
        copy_principal(matrix, probe_indices_, principal_);
        if (factorization_.factorize_inplace(principal_) == 0) {
            COPOSIT_UPPER_ENDPOINT_CBDD_DIAGNOSTICS("upper-endpoint-singular", probe_indices_.size());
            return true;
        }

        for (size_t row = 0; row < probe_indices_.size(); ++row) solution_(row, 0).set_one();
        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);

        bool all_nonpositive = true;
        for (size_t row = 0; row < probe_indices_.size(); ++row)
            all_nonpositive &= solution_(row, 0).sign() <= 0;
        if (all_nonpositive) {
            COPOSIT_UPPER_ENDPOINT_CBDD_DIAGNOSTICS("upper-endpoint-negative-witness", probe_indices_.size());
            return false;
        }

        const auto sizes = diagnostics_.active()
                               ? build_certificate<true>(matrix, probe_indices_, solution_, probe_lower_, probe_upper_)
                               : build_certificate<false>(matrix, probe_indices_, solution_, probe_lower_, probe_upper_);
        supports_.add_interval(probe_lower_, probe_upper_);
        if (diagnostics_.active()) diagnostics_.decision_diagram_certificate(probe_indices_.size(), sizes.first, sizes.second);
        COPOSIT_UPPER_ENDPOINT_CBDD_DIAGNOSTICS("upper-endpoint-certificate", probe_indices_.size());
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
    std::vector<integer> product_;
    std::vector<size_t> indices_;
    std::vector<size_t> probe_indices_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    interval_cbdd supports_;
    support lower_;
    support upper_;
    support probe_lower_;
    support probe_upper_;
    std::set<support> probed_upper_endpoints_;
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

#ifdef COPOSIT_UPPER_ENDPOINT_CBDD_DICKINSON_TESTING
std::pair<size_t, size_t> upper_endpoint_cbdd_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_cbdd diagram(dimension);
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
    return {count, diagram.node_count()};
}

size_t upper_endpoint_cbdd_maximum_interval_chain(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_cbdd diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.maximum_chain_length();
}
#endif

} // namespace coposit::model
