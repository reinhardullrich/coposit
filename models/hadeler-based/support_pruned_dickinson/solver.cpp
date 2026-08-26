#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class support_generator {
public:
    explicit support_generator(support_context& context)
        : context_(context)
        , dimension_(context.dimension())
        , forbidden_by_lowest_(dimension_)
        , partial_support_(context.make())
    {
    }

    template<class Callback>
    void generate(Callback&& callback)
    {
        for (target_cardinality_ = 1; target_cardinality_ <= dimension_; ++target_cardinality_) {
            activate_pending();
            emitted_ = false;
            if (!generate_from(dimension_, target_cardinality_, callback)) return;
            if (!emitted_) return;
        }
    }

    void add_forbidden(const support& forbidden)
    {
        pending_forbidden_.push_back(context_.clone(forbidden));
    }

private:
    void activate_pending()
    {
        for (support& forbidden : pending_forbidden_) {
            forbidden_by_lowest_[context_.first(forbidden)].push_back(std::move(forbidden));
        }
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit]) {
            if (context_.is_subset_of(forbidden, partial_support_)) return true;
        }
        return false;
    }

    template<class Callback>
    bool generate_from(size_t bits_remaining, size_t needed, Callback& callback)
    {
        timeout_checkpoint();
        if (needed == 0) {
            emitted_ = true;
            return callback(partial_support_, target_cardinality_);
        }
        if (needed > bits_remaining) return true;

        const size_t bit = bits_remaining - 1;
        if (needed < bits_remaining && !generate_from(bit, needed, callback)) return false;

        context_.set(partial_support_, bit);
        const bool keep_going = completes_forbidden(bit) || generate_from(bit, needed - 1, callback);
        context_.reset(partial_support_, bit);
        return keep_going;
    }

    support_context& context_;
    size_t dimension_;
    std::vector<std::vector<support>> forbidden_by_lowest_;
    std::vector<support> pending_forbidden_;
    support partial_support_;
    size_t target_cardinality_ = 0;
    bool emitted_ = false;
};

struct certificate_signature {
    explicit certificate_signature(support_context& context)
        : nonzero_support(context.make())
        , product_nonnegative(context.make())
    {
    }

    support nonzero_support;
    support product_nonnegative;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
        , mode_(mode)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        support_generator generator(support_context_);
        bool result = true;
        generator.generate([&](const support& current_support, size_t subset_dimension) {
            support_context_.extract_set_indices(current_support, indices_);
            if (subset_dimension <= 3) {
                if (classification_ != nullptr) {
                    const copositivity_classification face =
                        small_copositivity::classify_principal(matrix, indices_.data(), subset_dimension);
                    classification_->is_strictly_copositive &= face.is_strictly_copositive;
                    if (!face.is_copositive) {
                        result = false;
                        return false;
                    }
                } else if (!small_copositivity::check_principal(matrix, indices_.data(), subset_dimension, mode_)) {
                    result = false;
                    return false;
                }
            }
            if (!is_covered(current_support, indices_) && !process_subset(matrix, indices_, generator)) {
                result = false;
                return false;
            }
            return true;
        });
        return result;
    }

private:
    bool is_covered(const support& current_support, const std::vector<size_t>& indices) const
    {
        for (const size_t lowest : indices) {
            const std::vector<certificate_signature>& certificates = certificates_by_lowest_[lowest];
            for (auto certificate = certificates.rbegin(); certificate != certificates.rend(); ++certificate) {
                timeout_checkpoint();
                if (support_context_.is_subset_of(certificate->nonzero_support, current_support)
                    && support_context_.is_subset_of(current_support, certificate->product_nonnegative)) return true;
            }
        }
        return false;
    }

    bool process_subset(const matrix_integer& matrix, const std::vector<size_t>& indices, support_generator& generator)
    {
        const size_t dimension = indices.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (!singular) {
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
        } else {
            factorization_.one_nullspace_vector(solution_, principal_);

            bool has_positive_entry = false;
            for (size_t row = 0; row < dimension; ++row) {
                has_positive_entry |= solution_(row, 0).sign() > 0;
            }
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

        add_certificate(matrix, indices, solution_, generator);
        return true;
    }

    void add_certificate(
        const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution, support_generator& generator)
    {
        certificate_signature certificate(support_context_);
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) support_context_.set(certificate.nonzero_support, indices[local]);
        }

        bool covers_every_superset = true;
        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local) {
                product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            }
            if (product_[row].sign() >= 0) {
                support_context_.set(certificate.product_nonnegative, row);
            } else {
                covers_every_superset = false;
            }
        }

        if (covers_every_superset) generator.add_forbidden(certificate.nonzero_support);
        certificates_by_lowest_[support_context_.first(certificate.nonzero_support)].push_back(std::move(certificate));
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) {
                principal(row, column) = matrix(indices[row], indices[column]);
            }
        }
    }

    support_context support_context_;
    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
    std::vector<size_t> indices_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::check(matrix, mode);

    return dickinson_checker(dimension, mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::classify(matrix);

    copositivity_classification result{true, true};
    if (!dickinson_checker(dimension, result).check(matrix)) result = {false, false};
    return result;
}

} // namespace coposit::model
