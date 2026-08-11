#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/timeout.hpp>

#include "../source_trace.hpp"

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace coposit::model {

namespace {

/*
 * FracESSA's last optimized Hadeler checker before the cone replacement (commit 36902a3d). Principal subsets remain in increasing
 * cardinality and numeric-mask order. The dynamic index vector removes only the former fixed-width mask limit.
 */
class hadeler_checker {
public:
    hadeler_checker(size_t dimension, copositivity_mode mode) : factorization_(dimension), mode_(mode) {}
    hadeler_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension), mode_(copositivity_mode::copositive), classification_(&classification)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const size_t matrix_dimension = matrix.rows();
        for (size_t subset_dimension = 1; subset_dimension <= matrix_dimension; ++subset_dimension) {
            std::vector<size_t> indices(subset_dimension);
            for (size_t i = 0; i < subset_dimension; ++i) indices[i] = i;

            do {
                timeout_checkpoint();
                if (subset_dimension == 2) COPOSIT_SOURCE_TRACE("pair", indices[0], indices[1]);
                if (!check_subset(matrix, indices)) return false;
            } while (advance_numeric_mask_order(indices, matrix_dimension));
        }
        return true;
    }

private:
    static bool advance_numeric_mask_order(std::vector<size_t>& indices, size_t matrix_dimension)
    {
        for (size_t i = 0; i < indices.size(); ++i) {
            const size_t upper_bound = i + 1 < indices.size() ? indices[i + 1] : matrix_dimension;
            if (indices[i] + 1 == upper_bound) continue;

            ++indices[i];
            for (size_t reset = 0; reset < i; ++reset) indices[reset] = reset;
            return true;
        }
        return false;
    }

    bool check_subset(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        const size_t dimension = indices.size();
        if (dimension <= 3 && classification_ != nullptr) {
            const copositivity_classification subset = small_copositivity::classify_principal(matrix, indices.data(), dimension);
            classification_->is_strictly_copositive &= subset.is_strictly_copositive;
            return subset.is_copositive;
        }
        if (dimension <= 3) return small_copositivity::check_principal(matrix, indices.data(), dimension, mode_);

        principal_.resize(dimension, dimension);
        copy_principal(matrix, indices, principal_);

        const bool nonsingular = factorization_.factorize_inplace(principal_) != 0;
        const int determinant_sign = factorization_.determinant().sign();
        if (determinant_sign > 0
            || (classification_ == nullptr && mode_ == copositivity_mode::copositive && determinant_sign == 0)) return true;

        if (nonsingular) {
            // One retained solve C y = -1 replaces the former full inverse/adjugate construction.
            solution_.resize(dimension, 1);
            const integer minus_one(-1);
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0) = minus_one;

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
            for (size_t row = 0; row < dimension; ++row) {
                if (solution_(row, 0).sign() <= 0) return true;
            }
            return false;
        }

        if ((classification_ != nullptr && !classification_->is_strictly_copositive)
            || (classification_ == nullptr && mode_ == copositivity_mode::copositive)
            || dimension - factorization_.rank() != 1) return true;

        solution_.resize(dimension, 1);
        factorization_.one_nullspace_vector(solution_, principal_);
        const int basis_sign = solution_(0, 0).sign();
        if (basis_sign == 0) return true;
        for (size_t row = 1; row < dimension; ++row) {
            if (solution_(row, 0).sign() != basis_sign) return true;
        }
        if (classification_ != nullptr) {
            classification_->is_strictly_copositive = false;
            return true;
        }
        return false;
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

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
};

size_t validate_input(const matrix_integer& matrix)
{
    const size_t dimension = matrix.rows();
    if (dimension == 0 || matrix.cols() != dimension) throw std::invalid_argument("matrix must be nonempty and square");

    for (size_t i = 0; i < dimension; ++i) {
        for (size_t j = i + 1; j < dimension; ++j) {
            if (matrix(i, j).compare(matrix(j, i)) != 0) throw std::invalid_argument("matrix must be symmetric");
        }
    }
    return dimension;
}

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = validate_input(matrix);

    if (dimension <= 3) return small_copositivity::check(matrix, mode);

    return hadeler_checker(dimension, mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    const size_t dimension = validate_input(matrix);
    if (dimension <= 3) return small_copositivity::classify(matrix);

    copositivity_classification result{true, true};
    if (!hadeler_checker(dimension, result).check(matrix)) result = {false, false};
    return result;
}

} // namespace coposit::model
