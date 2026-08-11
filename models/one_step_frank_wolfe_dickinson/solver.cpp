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

struct certificate_signature {
    explicit certificate_signature(size_t dimension)
        : nonzero_support(dimension)
        , product_nonnegative(dimension)
    {
    }

    support nonzero_support;
    support product_nonnegative;
};

#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
bool last_one_step_witness = false;
bool last_one_step_line = false;
#endif

bool has_exact_one_step_frank_wolfe_witness(const matrix_integer& matrix)
{
    // ponytail: one O(n^2) exact centre-to-vertex scan; add directions only as a separate benchmark model.
    const size_t dimension = matrix.rows();
    const unsigned long dimension_value = static_cast<unsigned long>(dimension);
    std::vector<integer> row_sums(dimension);
    integer total;

    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        if (matrix(i, i).sign() <= 0) {
#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
            last_one_step_witness = true;
#endif
            return true;
        }
        row_sums[i] += matrix(i, i);
        total += matrix(i, i);
        for (size_t j = i + 1; j < dimension; ++j) {
            row_sums[i] += matrix(i, j);
            row_sums[j] += matrix(i, j);
            total += matrix(i, j);
            total += matrix(i, j);
        }
    }

    if (total.sign() <= 0) {
#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
        last_one_step_witness = true;
#endif
        return true;
    }

    size_t toward = 0;
    for (size_t i = 1; i < dimension; ++i) {
        if (row_sums[i].compare(row_sums[toward]) < 0) toward = i;
    }

    integer scaled_row_sum(row_sums[toward]);
    scaled_row_sum.multiply(dimension_value);
    integer descent_numerator;
    descent_numerator.set_difference(total, scaled_row_sum);
    if (descent_numerator.sign() <= 0) return false;

    integer curvature_numerator(matrix(toward, toward));
    curvature_numerator.multiply(dimension_value);
    curvature_numerator.multiply(dimension_value);
    integer work(row_sums[toward]);
    work.multiply(dimension_value);
    work.multiply(2);
    curvature_numerator -= work;
    curvature_numerator += total;

    // Nonpositive curvature and alpha >= 1 minimize at an endpoint. The exact scan already proved both endpoints positive.
    if (curvature_numerator.sign() <= 0 || descent_numerator.compare(curvature_numerator) >= 0) return false;

#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
    last_one_step_line = true;
#endif

    // For alpha = p/q, positive scaling by n*q gives z = (q-p)*1 + n*p*e_j.
    integer centre_weight;
    centre_weight.set_difference(curvature_numerator, descent_numerator);
    integer selected_weight(descent_numerator);
    selected_weight.multiply(dimension_value);

    integer coefficient;
    integer value;
    coefficient.set_product(centre_weight, centre_weight);
    value.set_product(coefficient, total);
    coefficient.set_product(centre_weight, selected_weight);
    coefficient.multiply(2);
    value.addmul(coefficient, row_sums[toward]);
    coefficient.set_product(selected_weight, selected_weight);
    value.addmul(coefficient, matrix(toward, toward));

    if (value.sign() > 0) return false;
#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
    last_one_step_witness = true;
#endif
    return true;
}

class dickinson_checker {
public:
    explicit dickinson_checker(size_t dimension)
        : factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const size_t matrix_dimension = matrix.rows();
        for (size_t subset_dimension = 1; subset_dimension <= matrix_dimension; ++subset_dimension) {
            std::vector<size_t> indices(subset_dimension);
            support current_support(matrix_dimension);
            for (size_t i = 0; i < subset_dimension; ++i) {
                indices[i] = i;
                current_support.set(i);
            }

            do {
                timeout_checkpoint();
                if (subset_dimension <= 3
                    && !small_copositivity::check_principal(matrix, indices.data(), subset_dimension)) return false;
                if (!is_covered(current_support, indices) && !process_subset(matrix, indices)) return false;
            } while (advance_numeric_mask_order(indices, current_support, matrix_dimension));
        }

        return true;
    }

private:
    static bool advance_numeric_mask_order(std::vector<size_t>& indices, support& current_support, size_t matrix_dimension)
    {
        for (size_t i = 0; i < indices.size(); ++i) {
            const size_t upper_bound = i + 1 < indices.size() ? indices[i + 1] : matrix_dimension;
            if (indices[i] + 1 == upper_bound) continue;

            current_support.reset(indices[i]);
            ++indices[i];
            current_support.set(indices[i]);
            for (size_t reset = 0; reset < i; ++reset) {
                current_support.reset(indices[reset]);
                indices[reset] = reset;
                current_support.set(indices[reset]);
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
                if (certificate->nonzero_support.is_subset_of(current_support)
                    && current_support.is_subset_of(certificate->product_nonnegative)) return true;
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
        if (all_nonnegative) return false;

        add_certificate(matrix, indices, solution_);
        return true;
    }

    void add_certificate(const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution)
    {
        certificate_signature certificate(matrix.rows());
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) certificate.nonzero_support.set(indices[local]);
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local) {
                product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            }
            if (product_[row].sign() >= 0) certificate.product_nonnegative.set(row);
        }

        certificates_by_lowest_[certificate.nonzero_support.lowest_index()].push_back(std::move(certificate));
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
    std::vector<integer> product_;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension == 0 || matrix.cols() != dimension) throw std::invalid_argument("matrix must be nonempty and square");

    for (size_t i = 0; i < dimension; ++i) {
        for (size_t j = i + 1; j < dimension; ++j) {
            if (matrix(i, j).compare(matrix(j, i)) != 0) throw std::invalid_argument("matrix must be symmetric");
        }
    }

#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
    last_one_step_witness = false;
    last_one_step_line = false;
#endif
    if (dimension <= 3) return small_copositivity::check(matrix);
    if (has_exact_one_step_frank_wolfe_witness(matrix)) return false;

    return dickinson_checker(dimension).check(matrix);
}

#ifdef COPOSIT_ONE_STEP_FRANK_WOLFE_DICKINSON_TESTING
bool one_step_frank_wolfe_witness_found_for_testing() noexcept
{
    return last_one_step_witness;
}

bool one_step_frank_wolfe_line_used_for_testing() noexcept
{
    return last_one_step_line;
}
#endif

} // namespace coposit::model
