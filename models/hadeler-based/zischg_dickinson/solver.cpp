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

#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
size_t level_two_skips = 0;
#endif

class negative_graph {
public:
    negative_graph(const matrix_integer& matrix, support_context& context)
        : context_(context)
        , unreached_(context.make())
        , frontier_(context.make())
        , next_(context.make())
    {
        neighbors_.reserve(matrix.rows());
        for (size_t vertex = 0; vertex < matrix.rows(); ++vertex) neighbors_.push_back(context.make());
        frontier_indices_.reserve(matrix.rows());

        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = row + 1; column < matrix.rows(); ++column) {
                if (matrix(row, column).sign() < 0) {
                    context_.set(neighbors_[row], column);
                    context_.set(neighbors_[column], row);
                } else {
                    complete_ = false;
                }
            }
        }
    }

    bool induced_is_connected(const support& vertices)
    {
        if (complete_) return true;
        context_.copy(unreached_, vertices);
        const size_t root = context_.first(unreached_);
        context_.reset(unreached_, root);
        if (context_.empty(unreached_)) return true;
        if (context_.is_subset_of(unreached_, neighbors_[root])) return true;

        context_.clear(frontier_);
        context_.set(frontier_, root);
        while (!context_.empty(unreached_)) {
            timeout_checkpoint();
            context_.clear(next_);
            context_.extract_set_indices(frontier_, frontier_indices_);
            for (const size_t vertex : frontier_indices_) context_.add(next_, neighbors_[vertex]);
            context_.intersect(next_, unreached_);
            if (context_.empty(next_)) return false;
            context_.subtract(unreached_, next_);
            context_.swap(frontier_, next_);
        }
        return true;
    }

private:
    support_context& context_;
    std::vector<support> neighbors_;
    support unreached_;
    support frontier_;
    support next_;
    std::vector<size_t> frontier_indices_;
    bool complete_ = true;
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
    dickinson_checker(const matrix_integer& matrix, copositivity_mode mode)
        : support_context_(matrix.rows())
        , factorization_(matrix.rows())
        , product_(matrix.rows())
        , certificates_by_lowest_(matrix.rows())
        , negative_graph_(matrix, support_context_)
        , mode_(mode)
    {
    }


    dickinson_checker(const matrix_integer& matrix, copositivity_classification& classification)
        : support_context_(matrix.rows())
        , factorization_(matrix.rows())
        , product_(matrix.rows())
        , certificates_by_lowest_(matrix.rows())
        , negative_graph_(matrix, support_context_)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const size_t matrix_dimension = matrix.rows();
        for (size_t subset_dimension = 1; subset_dimension <= matrix_dimension; ++subset_dimension) {
            std::vector<size_t> indices(subset_dimension);
            support current_support = support_context_.make();
            for (size_t i = 0; i < subset_dimension; ++i) {
                indices[i] = i;
                support_context_.set(current_support, i);
            }

            do {
                timeout_checkpoint();
                if (subset_dimension <= 3) {
                    if (classification_ != nullptr) {
                        const copositivity_classification face =
                            small_copositivity::classify_principal(matrix, indices.data(), subset_dimension);
                        classification_->is_strictly_copositive &= face.is_strictly_copositive;
                        if (!face.is_copositive) return false;
                    } else if (!small_copositivity::check_principal(matrix, indices.data(), subset_dimension, mode_)) {
                        return false;
                    }
                }
                if (subset_dimension > 3 && !negative_graph_.induced_is_connected(current_support)) {
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
                    ++level_two_skips;
#endif
                    continue;
                }
                if (is_covered(current_support, indices)) continue;
                if (!process_subset(matrix, indices)) return false;
            } while (advance_numeric_mask_order(indices, current_support, matrix_dimension));
            support_context_.release(std::move(current_support));
        }
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

        add_certificate(matrix, indices, solution_);
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
            for (size_t local = 0; local < indices.size(); ++local) product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            if (product_[row].sign() >= 0) support_context_.set(certificate.product_nonnegative, row);
        }

        certificates_by_lowest_[support_context_.first(certificate.nonzero_support)].push_back(std::move(certificate));
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    support_context support_context_;
    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
    negative_graph negative_graph_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
};

} // namespace

#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
size_t level_two_skips_for_testing() noexcept
{
    return level_two_skips;
}
#endif

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::check(matrix, mode);
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
    level_two_skips = 0;
#endif
    return dickinson_checker(matrix, mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::classify(matrix);
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
    level_two_skips = 0;
#endif
    copositivity_classification result{true, true};
    if (!dickinson_checker(matrix, result).check(matrix)) result = {false, false};
    return result;
}

} // namespace coposit::model
