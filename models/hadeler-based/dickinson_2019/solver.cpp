#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "../../baselines/source_diagnostics.hpp"

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

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
        , diagnostics_(diagnostics::metric::support, dimension)
    {
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const size_t matrix_dimension = matrix.rows();
        for (size_t subset_dimension = 1; subset_dimension <= matrix_dimension; ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            std::vector<size_t> indices(subset_dimension);
            support current_support = support_context_.make();
            for (size_t i = 0; i < subset_dimension; ++i) {
                indices[i] = i;
                support_context_.set(current_support, i);
            }

            do {
                timeout_checkpoint();
                diagnostics_.visit_support();
                const bool covered = is_covered(current_support, indices);
                COPOSIT_SOURCE_DIAGNOSTICS(covered ? "covered" : "process", subset_dimension);
                if (covered) {
                    diagnostics_.covered_support();
                } else {
                    diagnostics_.secondary();
                    if (!process_subset(matrix, indices)) {
                        support_context_.release(std::move(current_support));
                        diagnostics_.finish();
                        return false;
                    }
                }
            } while (advance_numeric_mask_order(indices, current_support, matrix_dimension));
            support_context_.release(std::move(current_support));
        }

        diagnostics_.finish();
        return true;
    }

private:
    bool advance_numeric_mask_order(std::vector<size_t>& indices, support& current_support, size_t matrix_dimension) const
    {
        for (size_t i = 0; i < indices.size(); ++i) {
            const size_t upper_bound = i + 1 < indices.size() ? indices[i + 1] : matrix_dimension;
            if (indices[i] + 1 == upper_bound) continue;

            support_context_.reset(current_support, indices[i]);
            ++indices[i];
            support_context_.set(current_support, indices[i]);
            for (size_t reset = 0; reset < i; ++reset) {
                support_context_.reset(current_support, indices[reset]);
                indices[reset] = reset;
                support_context_.set(current_support, indices[reset]);
            }
            return true;
        }
        return false;
    }

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

    bool process_subset(const matrix_integer& matrix, const std::vector<size_t>& indices)
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

        add_certificate(matrix, indices, solution_);
        diagnostics_.certificate();
        return true;
    }

    void add_certificate(const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution)
    {
        certificate_signature certificate(support_context_);
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) support_context_.set(certificate.nonzero_support, indices[local]);
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local) {
                product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            }
            if (product_[row].sign() >= 0) support_context_.set(certificate.product_nonnegative, row);
        }

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
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    return dickinson_checker(dimension, mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    copositivity_classification result{true, true};
    if (!dickinson_checker(dimension, result).check(matrix)) result = {false, false};
    return result;
}

} // namespace coposit::model
