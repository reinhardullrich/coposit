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
#include <limits>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

uint64_t saturated_binomial(size_t n, size_t k) noexcept
{
    if (k > n) return 0;
    k = std::min(k, n - k);
    uint64_t result = 1;
    for (size_t i = 1; i <= k; ++i) {
        size_t numerator = n - k + i;
        size_t denominator = i;
        const size_t numerator_divisor = std::gcd(numerator, denominator);
        numerator /= numerator_divisor;
        denominator /= numerator_divisor;
        const uint64_t result_divisor = std::gcd(result, static_cast<uint64_t>(denominator));
        result /= result_divisor;
        denominator /= static_cast<size_t>(result_divisor);
        assert(denominator == 1);
        if (numerator != 0 && result > std::numeric_limits<uint64_t>::max() / numerator)
            return std::numeric_limits<uint64_t>::max();
        result *= numerator;
    }
    return result;
}

class support_generator {
public:
    explicit support_generator(support_context& context, diagnostics::tracker* diagnostics = nullptr)
        : support_context_(context)
        , dimension_(context.dimension())
        , forbidden_by_lowest_(context.dimension())
        , partial_support_(support_context_.make())
        , diagnostics_(diagnostics)
    {
    }

    template <class Callback>
    bool generate(Callback&& callback)
    {
        for (target_cardinality_ = 1; target_cardinality_ <= dimension_; ++target_cardinality_) {
            activate_pending();
            emitted_ = false;
            if (diagnostics_ != nullptr) diagnostics_->support_cardinality(target_cardinality_);
            if (!generate_from(dimension_, target_cardinality_, callback)) return false;
            if (!emitted_) {
                if (diagnostics_ != nullptr)
                    for (size_t remaining = target_cardinality_ + 1; remaining <= dimension_; ++remaining)
                        diagnostics_->skip_supports(saturated_binomial(dimension_, remaining));
                return true;
            }
        }
        return true;
    }

    bool add_forbidden(support lower)
    {
        assert(!support_context_.empty(lower));
        for (const support& retained : pending_forbidden_)
            if (support_context_.is_subset_of(retained, lower)) {
                support_context_.release(std::move(lower));
                return false;
            }
        std::vector<support> retained_values;
        retained_values.reserve(pending_forbidden_.size() + 1);
        for (support& retained : pending_forbidden_) {
            if (support_context_.is_subset_of(lower, retained)) support_context_.release(std::move(retained));
            else retained_values.push_back(std::move(retained));
        }
        pending_forbidden_.swap(retained_values);
        pending_forbidden_.push_back(std::move(lower));
        return true;
    }

    bool is_actively_forbidden(const support& candidate) const noexcept
    {
        for (size_t lowest = 0; lowest < dimension_; ++lowest) {
            if (!support_context_.contains(candidate, lowest)) continue;
            for (const support& forbidden : forbidden_by_lowest_[lowest])
                if (support_context_.is_subset_of(forbidden, candidate)) return true;
        }
        return false;
    }

private:
    void activate_pending()
    {
        for (support& forbidden : pending_forbidden_)
            forbidden_by_lowest_[support_context_.first(forbidden)].push_back(std::move(forbidden));
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit])
            if (support_context_.is_subset_of(forbidden, partial_support_)) return true;
        return false;
    }

    template <class Callback>
    bool generate_from(size_t bits_remaining, size_t needed, Callback& callback)
    {
        timeout_checkpoint();
        if (needed == 0) {
            emitted_ = true;
            if (diagnostics_ != nullptr) diagnostics_->visit_support();
            return callback(partial_support_, target_cardinality_);
        }
        if (needed > bits_remaining) return true;

        const size_t bit = bits_remaining - 1;
        if (needed < bits_remaining && !generate_from(bit, needed, callback)) return false;

        support_context_.set(partial_support_, bit);
        bool keep_going = true;
        if (completes_forbidden(bit)) {
            if (diagnostics_ != nullptr) diagnostics_->skip_supports(saturated_binomial(bit, needed - 1));
        } else {
            keep_going = generate_from(bit, needed - 1, callback);
        }
        support_context_.reset(partial_support_, bit);
        return keep_going;
    }

    support_context& support_context_;
    size_t dimension_;
    std::vector<std::vector<support>> forbidden_by_lowest_;
    std::vector<support> pending_forbidden_;
    support partial_support_;
    diagnostics::tracker* diagnostics_;
    size_t target_cardinality_ = 0;
    bool emitted_ = false;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : support_context_(dimension)
        , factorization_(dimension)
        , lifted_seen_(support_less{&support_context_})
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
        , supports_(support_context_, diagnostics_.active() ? &diagnostics_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , lifted_seen_(support_less{&support_context_})
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
        , supports_(support_context_, diagnostics_.active() ? &diagnostics_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        const bool result = supports_.generate([&](const support& current, size_t cardinality) {
            timeout_checkpoint();
            lift_root_cardinality_ = cardinality;
            diagnostics_.support_lift_idle(lifted_seen_.size());
            support_context_.extract_set_indices(current, indices_);
            COPOSIT_LAYERED_LIFT_DIAGNOSTICS("process", cardinality);
            return process_subset(matrix);
        });
        diagnostics_.finish();
        return result;
    }

#ifdef COPOSIT_LAYERED_SINGULAR_LIFT_DICKINSON_TESTING
    bool lift_for_test(const matrix_integer& matrix, const std::vector<size_t>& root)
    {
        indices_ = root;
        lift_root_cardinality_ = root.size();
        diagnostics_.support_cardinality(root.size());
        principal_.resize(root.size(), root.size());
        copy_principal(matrix, indices_, principal_);
        if (factorization_.factorize_inplace(principal_) != 0 || root.size() - factorization_.rank() <= 1) return false;
        return lift_singular_support(matrix);
    }
#endif

private:
    bool process_subset(const matrix_integer& matrix)
    {
        diagnostics_.secondary();
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        const size_t nullity = singular ? dimension - factorization_.rank() : 0;
        if (!singular) {
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
        } else {
            factorization_.one_nullspace_vector(solution_, principal_);
            orient_toward_positive(solution_);
        }

        bool all_nonpositive = true;
        bool all_nonnegative = singular;
        for (size_t row = 0; row < dimension; ++row) {
            all_nonpositive &= solution_(row, 0).sign() <= 0;
            all_nonnegative &= solution_(row, 0).sign() >= 0;
        }
        if (all_nonpositive) return false;
        if (all_nonnegative && !record_non_strict_zero()) return false;

        add_ceiling_certificate(matrix, false);
        return !singular || nullity <= 1 || lift_singular_support(matrix);
    }

    bool lift_singular_support(const matrix_integer& matrix)
    {
        support root = support_context_.make();
        for (const size_t index : indices_) support_context_.set(root, index);
        if (lifted_seen_.find(root) != lifted_seen_.end()) {
            support_context_.release(std::move(root));
            diagnostics_.support_lift_duplicate(indices_.size(), 0, lifted_seen_.size());
            diagnostics_.support_lift_idle(lifted_seen_.size());
            return true;
        }
        lifted_seen_.insert(std::move(root));
        const bool result = lift_children(matrix);
        diagnostics_.support_lift_idle(lifted_seen_.size());
        return result;
    }

    bool lift_children(const matrix_integer& matrix)
    {
        for (size_t candidate = 0; candidate < matrix.rows(); ++candidate) {
            timeout_checkpoint();
            const auto position = std::lower_bound(indices_.begin(), indices_.end(), candidate);
            if (position != indices_.end() && *position == candidate) continue;
            const size_t insertion = static_cast<size_t>(position - indices_.begin());
            indices_.insert(position, candidate);

            const size_t dimension = indices_.size();
            support lifted = support_context_.make();
            for (const size_t index : indices_) support_context_.set(lifted, index);
            bool keep_going = true;
            const size_t depth = dimension - lift_root_cardinality_;
            if (supports_.is_actively_forbidden(lifted)) {
                diagnostics_.support_lift_covered(dimension, depth, lifted_seen_.size());
                support_context_.release(std::move(lifted));
            } else if (lifted_seen_.find(lifted) != lifted_seen_.end()) {
                support_context_.release(std::move(lifted));
                diagnostics_.support_lift_duplicate(dimension, depth, lifted_seen_.size());
            } else {
                lifted_seen_.insert(std::move(lifted));
                diagnostics_.support_lift_system(dimension, depth, lifted_seen_.size());
                principal_.resize(dimension, dimension);
                copy_principal(matrix, indices_, principal_);
                if (factorization_.factorize_inplace(principal_) == 0) {
                    const size_t nullity = dimension - factorization_.rank();
                    COPOSIT_LAYERED_LIFT_DIAGNOSTICS("lift", dimension);
                    if (nullity == 1)
                        keep_going = process_lifted_nullity_one(matrix);
                    else
                        keep_going = lift_children(matrix);
                }
            }

            indices_.erase(indices_.begin() + static_cast<std::ptrdiff_t>(insertion));
            if (!keep_going) return false;
        }
        return true;
    }

    bool process_lifted_nullity_one(const matrix_integer& matrix)
    {
        solution_.resize(indices_.size(), 1);
        factorization_.one_nullspace_vector(solution_, principal_);
        const bool mixed_sign = orient_toward_positive(solution_);
        if (!mixed_sign && !record_non_strict_zero()) return false;

        add_ceiling_certificate(matrix, true);
        if (mixed_sign) {
            solution_.negate();
            add_ceiling_certificate(matrix, true);
            solution_.negate();
        }
        return true;
    }

    static bool orient_toward_positive(matrix_integer& vector)
    {
        bool has_positive = false;
        bool has_negative = false;
        for (size_t row = 0; row < vector.rows(); ++row) {
            has_positive |= vector(row, 0).sign() > 0;
            has_negative |= vector(row, 0).sign() < 0;
        }
        assert(has_positive || has_negative);
        if (!has_positive) vector.negate();
        return has_positive && has_negative;
    }

    bool record_non_strict_zero()
    {
        if (classification_ != nullptr) {
            classification_->is_strictly_copositive = false;
            return true;
        }
        return mode_ != copositivity_mode::strictly_copositive;
    }

    void add_ceiling_certificate(const matrix_integer& matrix, bool lifted)
    {
        size_t support_row = 0;
        for (size_t row = 0; row < matrix.rows(); ++row) {
            if (support_row < indices_.size() && indices_[support_row] == row) {
                ++support_row;
                continue;
            }
            timeout_checkpoint();
            product_.set_zero();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_.addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_.sign() < 0) {
                COPOSIT_LAYERED_LIFT_DIAGNOSTICS(lifted ? "discard-lift" : "discard-certificate", indices_.size());
                return;
            }
        }

        support lower = support_context_.make();
        size_t lower_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (solution_(local, 0).is_zero()) continue;
            support_context_.set(lower, indices_[local]);
            ++lower_size;
        }

        if (!supports_.add_forbidden(std::move(lower))) return;
        diagnostics_.lifted_certificate(indices_.size(), matrix.rows(), lower_size);
        COPOSIT_LAYERED_LIFT_DIAGNOSTICS(lifted ? "lift-ceiling-certificate" : "ceiling-certificate", lower_size);
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    support_context support_context_;
    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    integer product_;
    std::vector<size_t> indices_;
    std::set<support, support_less> lifted_seen_;
    size_t lift_root_cardinality_ = 0;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    support_generator supports_;
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

#ifdef COPOSIT_LAYERED_SINGULAR_LIFT_DICKINSON_TESTING
bool layered_singular_lift_for_test(const matrix_integer& matrix, const std::vector<size_t>& root)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive).lift_for_test(matrix, root);
}

std::vector<uint64_t> layered_generated_masks(size_t dimension, uint64_t forbidden_trigger)
{
    assert(dimension <= 64);
    support_context context(dimension);
    support_generator generator(context);
    std::vector<uint64_t> result;
    generator.generate([&](const support& current, size_t) {
        uint64_t mask = 0;
        for (size_t bit = 0; bit < dimension; ++bit)
            if (context.contains(current, bit)) mask |= uint64_t{1} << bit;
        result.push_back(mask);
        if (mask == forbidden_trigger) generator.add_forbidden(context.clone(current));
        return true;
    });
    return result;
}
#endif

} // namespace coposit::model
