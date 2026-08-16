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
#include <limits>
#include <numeric>
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
    explicit support_generator(size_t dimension, diagnostics::tracker* diagnostics = nullptr)
        : dimension_(dimension)
        , forbidden_by_lowest_(dimension)
        , partial_support_(dimension)
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

    void add_forbidden(support lower)
    {
        assert(!lower.empty());
        pending_forbidden_.push_back(std::move(lower));
    }

private:
    void activate_pending()
    {
        for (support& forbidden : pending_forbidden_)
            forbidden_by_lowest_[forbidden.lowest_index()].push_back(std::move(forbidden));
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit])
            if (forbidden.is_subset_of(partial_support_)) return true;
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

        partial_support_.set(bit);
        bool keep_going = true;
        if (completes_forbidden(bit)) {
            if (diagnostics_ != nullptr) diagnostics_->skip_supports(saturated_binomial(bit, needed - 1));
        } else {
            keep_going = generate_from(bit, needed - 1, callback);
        }
        partial_support_.reset(bit);
        return keep_going;
    }

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
        : factorization_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
        , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
        , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        const bool result = supports_.generate([&](const support& current, size_t cardinality) {
            timeout_checkpoint();
            current.copy_indices_to(indices_);
            COPOSIT_CEILING_DICKINSON_DIAGNOSTICS("process", cardinality);
            return process_subset(matrix);
        });
        diagnostics_.finish();
        return result;
    }

private:
    bool process_subset(const matrix_integer& matrix)
    {
        diagnostics_.secondary();
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

        add_ceiling_certificate(matrix);
        return true;
    }

    void add_ceiling_certificate(const matrix_integer& matrix)
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
                COPOSIT_CEILING_DICKINSON_DIAGNOSTICS("discard-certificate", indices_.size());
                return;
            }
        }

        support lower(matrix.rows());
        size_t lower_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (solution_(local, 0).is_zero()) continue;
            lower.set(indices_[local]);
            ++lower_size;
        }

        supports_.add_forbidden(std::move(lower));
        diagnostics_.certificate(matrix.rows() - lower_size);
        COPOSIT_CEILING_DICKINSON_DIAGNOSTICS("ceiling-certificate", lower_size);
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
    integer product_;
    std::vector<size_t> indices_;
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

#ifdef COPOSIT_CEILING_PRUNED_DICKINSON_TESTING
std::vector<uint64_t> ceiling_pruned_generated_masks(size_t dimension, uint64_t forbidden_trigger)
{
    assert(dimension <= 64);
    support_generator generator(dimension);
    std::vector<uint64_t> result;
    generator.generate([&](const support& current, size_t) {
        uint64_t mask = 0;
        for (size_t bit = 0; bit < dimension; ++bit)
            if (current.contains(bit)) mask |= uint64_t{1} << bit;
        result.push_back(mask);
        if (mask == forbidden_trigger) generator.add_forbidden(current);
        return true;
    });
    return result;
}
#endif

} // namespace coposit::model
