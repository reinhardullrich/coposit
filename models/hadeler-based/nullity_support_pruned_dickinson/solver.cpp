#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
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

struct signed_ratio {
    integer numerator;
    integer denominator;
};

struct coverage_score {
    integer future_supports;
    size_t width = 0;
    size_t upper_size = 0;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , candidate_product_(dimension)
        , certificates_by_lowest_(dimension)
        , mode_(mode)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , candidate_product_(dimension)
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

#ifdef COPOSIT_NULLITY_SUPPORT_PRUNED_DICKINSON_TESTING
    matrix_integer select_nullspace_vector_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        principal_.resize(indices.size(), indices.size());
        solution_.resize(indices.size(), 1);
        copy_principal(matrix, indices, principal_);
        if (factorization_.factorize_inplace(principal_) != 0) throw std::invalid_argument("principal matrix must be singular");
        if (!select_singular_solution(matrix, indices)) throw std::runtime_error("principal matrix has a nonnegative null vector");
        return solution_;
    }
#endif

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

        if (factorization_.factorize_inplace(principal_) == 0) {
            if (!select_singular_solution(matrix, indices)) {
                if (classification_ != nullptr) classification_->is_strictly_copositive = false;
                else if (mode_ == copositivity_mode::strictly_copositive) return false;
            }
            add_certificate(matrix, indices, solution_, generator, true);
            return true;
        }

        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();
        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);

        bool all_nonpositive = true;
        for (size_t row = 0; row < dimension; ++row) all_nonpositive &= solution_(row, 0).sign() <= 0;
        if (all_nonpositive) return false;

        add_certificate(matrix, indices, solution_, generator, false);
        return true;
    }

    bool select_singular_solution(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        const size_t dimension = indices.size();
        const size_t nullity = dimension - factorization_.rank();
        nullspace_basis_.resize(dimension, nullity);
        basis_products_.resize(matrix.rows(), nullity);
        candidate_.resize(dimension, 1);
        factorization_.nullspace_basis(nullspace_basis_, principal_);
        calculate_basis_products(matrix, indices);
        best_score_set_ = false;

        if (nullity == 1) {
            if (!consider_basis_column(0, dimension)) return false;
        } else if (nullity == 2) {
            if (!sweep_nullity_two(dimension)) return false;
        } else {
            for (size_t column = 0; column < nullity; ++column) {
                if (!consider_basis_column(column, dimension)) return false;
            }
        }

        assert(best_score_set_);
        return true;
    }

    void calculate_basis_products(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column < nullspace_basis_.cols(); ++column) {
                basis_products_(row, column).set_zero();
                for (size_t local = 0; local < indices.size(); ++local) {
                    basis_products_(row, column).addmul(matrix(row, indices[local]), nullspace_basis_(local, column));
                }
            }
        }
    }

    bool consider_basis_column(size_t column, size_t current_dimension)
    {
        for (size_t row = 0; row < candidate_.rows(); ++row) candidate_(row, 0) = nullspace_basis_(row, column);
        for (size_t row = 0; row < candidate_product_.size(); ++row) candidate_product_[row] = basis_products_(row, column);
        return consider_materialized_candidate(current_dimension);
    }

    bool consider_ratio(const signed_ratio& ratio, size_t current_dimension)
    {
        for (size_t row = 0; row < candidate_.rows(); ++row) {
            candidate_(row, 0).set_product(nullspace_basis_(row, 0), ratio.denominator);
            candidate_(row, 0).addmul(nullspace_basis_(row, 1), ratio.numerator);
        }
        for (size_t row = 0; row < candidate_product_.size(); ++row) {
            candidate_product_[row].set_product(basis_products_(row, 0), ratio.denominator);
            candidate_product_[row].addmul(basis_products_(row, 1), ratio.numerator);
        }
        return consider_materialized_candidate(current_dimension);
    }

    bool consider_materialized_candidate(size_t current_dimension)
    {
        bool all_nonnegative = true;
        bool all_nonpositive = true;
        size_t lower_size = 0;
        for (size_t row = 0; row < candidate_.rows(); ++row) {
            const int sign = candidate_(row, 0).sign();
            all_nonnegative &= sign >= 0;
            all_nonpositive &= sign <= 0;
            lower_size += sign != 0;
        }
        if (all_nonnegative || all_nonpositive) {
            for (size_t row = 0; row < candidate_.rows(); ++row) {
                solution_(row, 0) = candidate_(row, 0);
                if (all_nonpositive) solution_(row, 0).negate();
            }
            for (size_t row = 0; row < candidate_product_.size(); ++row) {
                product_[row] = candidate_product_[row];
                if (all_nonpositive) product_[row].negate();
            }
            return false;
        }

        size_t positive_upper_size = 0;
        size_t negative_upper_size = 0;
        for (const integer& value : candidate_product_) {
            positive_upper_size += value.sign() >= 0;
            negative_upper_size += value.sign() <= 0;
        }

        consider_orientation(1, lower_size, positive_upper_size, current_dimension);
        consider_orientation(-1, lower_size, negative_upper_size, current_dimension);
        return true;
    }

    void consider_orientation(int orientation, size_t lower_size, size_t upper_size, size_t current_dimension)
    {
        coverage_score score;
        set_future_support_count(score.future_supports, lower_size, upper_size, current_dimension);
        score.width = upper_size - lower_size;
        score.upper_size = upper_size;
        if (best_score_set_ && !score_is_better(score, best_score_)) return;

        best_score_ = score;
        best_score_set_ = true;
        for (size_t row = 0; row < candidate_.rows(); ++row) {
            solution_(row, 0) = candidate_(row, 0);
            if (orientation < 0) solution_(row, 0).negate();
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            product_[row] = candidate_product_[row];
            if (orientation < 0) product_[row].negate();
        }
    }

    static void set_future_support_count(integer& result, size_t lower_size, size_t upper_size, size_t current_dimension)
    {
        result.set_zero();
        if (upper_size <= current_dimension) return;

        const size_t optional = upper_size - lower_size;
        integer term;
        for (size_t support_size = current_dimension + 1; support_size <= upper_size; ++support_size) {
            fmpz_bin_uiui(term.native_handle(), static_cast<ulong>(optional), static_cast<ulong>(support_size - lower_size));
            result += term;
        }
    }

    static bool score_is_better(const coverage_score& candidate, const coverage_score& current)
    {
        const int future_comparison = candidate.future_supports.compare(current.future_supports);
        if (future_comparison != 0) return future_comparison > 0;
        if (candidate.width != current.width) return candidate.width > current.width;
        return candidate.upper_size > current.upper_size;
    }

    bool sweep_nullity_two(size_t current_dimension)
    {
        // ponytail: this materializes O(n) sign regions in O(n^2); switch to incremental counts only if nullity-two profiles dominate.
        if (!consider_basis_column(0, current_dimension) || !consider_basis_column(1, current_dimension)) return false;
        find_breakpoints();
        if (breakpoints_.empty()) return true;

        signed_ratio sample;
        sample.numerator = breakpoints_.front().numerator;
        sample.numerator -= breakpoints_.front().denominator;
        sample.denominator = breakpoints_.front().denominator;
        if (!consider_ratio(sample, current_dimension)) return false;

        for (size_t index = 0; index < breakpoints_.size(); ++index) {
            if (!consider_ratio(breakpoints_[index], current_dimension)) return false;
            if (index + 1 == breakpoints_.size()) continue;

            sample.numerator = breakpoints_[index].numerator;
            sample.numerator += breakpoints_[index + 1].numerator;
            sample.denominator = breakpoints_[index].denominator;
            sample.denominator += breakpoints_[index + 1].denominator;
            if (!consider_ratio(sample, current_dimension)) return false;
        }

        sample.numerator = breakpoints_.back().numerator;
        sample.numerator += breakpoints_.back().denominator;
        sample.denominator = breakpoints_.back().denominator;
        return consider_ratio(sample, current_dimension);
    }

    void find_breakpoints()
    {
        breakpoints_.clear();
        for (size_t row = 0; row < nullspace_basis_.rows(); ++row) {
            add_breakpoint(nullspace_basis_(row, 0), nullspace_basis_(row, 1));
        }
        for (size_t row = 0; row < basis_products_.rows(); ++row) {
            add_breakpoint(basis_products_(row, 0), basis_products_(row, 1));
        }

        integer left;
        integer right;
        const auto less = [&](const signed_ratio& first, const signed_ratio& second) {
            left.set_product(first.numerator, second.denominator);
            right.set_product(second.numerator, first.denominator);
            return left.compare(right) < 0;
        };
        const auto equal = [&](const signed_ratio& first, const signed_ratio& second) {
            left.set_product(first.numerator, second.denominator);
            right.set_product(second.numerator, first.denominator);
            return left.compare(right) == 0;
        };
        std::sort(breakpoints_.begin(), breakpoints_.end(), less);
        breakpoints_.erase(std::unique(breakpoints_.begin(), breakpoints_.end(), equal), breakpoints_.end());
    }

    void add_breakpoint(integer::const_reference base, integer::const_reference direction)
    {
        if (direction.is_zero()) return;
        signed_ratio breakpoint;
        breakpoint.denominator.set_abs(direction);
        if (direction.sign() > 0) {
            breakpoint.numerator = base;
            breakpoint.numerator.negate();
        } else {
            breakpoint.numerator = base;
        }
        breakpoints_.push_back(std::move(breakpoint));
    }

    void add_certificate(const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution,
                         support_generator& generator, bool product_is_ready)
    {
        certificate_signature certificate(support_context_);
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) support_context_.set(certificate.nonzero_support, indices[local]);
        }

        if (!product_is_ready) {
            for (integer& value : product_) value.set_zero();
            for (size_t row = 0; row < matrix.rows(); ++row) {
                timeout_checkpoint();
                for (size_t local = 0; local < indices.size(); ++local) {
                    product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
                }
            }
        }

        bool covers_every_superset = true;
        for (size_t row = 0; row < matrix.rows(); ++row) {
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
    matrix_integer nullspace_basis_;
    matrix_integer basis_products_;
    matrix_integer candidate_;
    std::vector<integer> product_;
    std::vector<integer> candidate_product_;
    std::vector<signed_ratio> breakpoints_;
    coverage_score best_score_;
    bool best_score_set_ = false;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
    std::vector<size_t> indices_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
};

} // namespace

#ifdef COPOSIT_NULLITY_SUPPORT_PRUNED_DICKINSON_TESTING
matrix_integer select_nullspace_vector_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::strictly_copositive).select_nullspace_vector_for_test(matrix, indices);
}
#endif

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
