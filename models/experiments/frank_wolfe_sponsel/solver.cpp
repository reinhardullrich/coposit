#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

#ifdef COPOSIT_FRANK_WOLFE_SPONSEL_TESTING
bool last_frank_wolfe_witness = false;
bool last_frank_wolfe_line_step = false;
#endif

enum class state { reject, accept, split };

struct evaluation {
    state result;
    size_t split_i;
    size_t split_j;
};

struct positive_ratio {
    integer numerator;
    integer denominator;
};

struct node {
    matrix_integer gram;
    size_t split_i;
    size_t split_j;
};

int compare(const positive_ratio& left, const positive_ratio& right)
{
    integer cross_left;
    integer cross_right;
    cross_left.set_product(left.numerator, right.denominator);
    cross_right.set_product(right.numerator, left.denominator);
    return cross_left.compare(cross_right);
}

bool fails_mode(integer::const_reference value, copositivity_mode mode) noexcept
{
    return value.sign() < (mode == copositivity_mode::copositive ? 0 : 1);
}

bool passes_h_certificate(const matrix_integer& gram, fraction_free_ldlt_factorization& factorization,
                          copositivity_mode mode)
{
    const size_t dimension = gram.rows();
    matrix_integer stripped(dimension, dimension);
    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        stripped(i, i) = gram(i, i);
        for (size_t j = i + 1; j < dimension; ++j) {
            if (gram(i, j).sign() < 0) {
                stripped(i, j) = gram(i, j);
                stripped(j, i) = gram(i, j);
            }
        }
    }

    factorization.factorize_inplace(stripped);
    return mode == copositivity_mode::copositive
        ? factorization.is_positive_semidefinite()
        : factorization.is_positive_definite();
}

bool has_exact_one_step_frank_wolfe_witness(const matrix_integer& gram, copositivity_mode mode)
{
    const size_t dimension = gram.rows();
    const unsigned long dimension_value = static_cast<unsigned long>(dimension);
    std::vector<integer> row_sums(dimension);
    integer total;

    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        row_sums[i] += gram(i, i);
        total += gram(i, i);
        for (size_t j = i + 1; j < dimension; ++j) {
            row_sums[i] += gram(i, j);
            row_sums[j] += gram(i, j);
            total += gram(i, j);
            total += gram(i, j);
        }
    }

    if (fails_mode(total, mode)) {
#ifdef COPOSIT_FRANK_WOLFE_SPONSEL_TESTING
        last_frank_wolfe_witness = true;
#endif
        return true;
    }

    size_t toward = 0;
    for (size_t i = 1; i < dimension; ++i) {
        if (row_sums[i].compare(row_sums[toward]) < 0) toward = i;
    }

    // At x = 1/n, the half directional derivative toward e_j is
    // (n * row_sum_j - 1^T G 1) / n^2. A positive descent numerator means that this direction decreases the quadratic.
    integer scaled_row_sum(row_sums[toward]);
    scaled_row_sum.multiply(dimension_value);
    integer descent_numerator;
    descent_numerator.set_difference(total, scaled_row_sum);
    if (descent_numerator.sign() <= 0) return false;

    // n^2 times the line curvature is n^2*g_jj - 2*n*row_sum_j + 1^T G 1.
    integer curvature_numerator(gram(toward, toward));
    curvature_numerator.multiply(dimension_value);
    curvature_numerator.multiply(dimension_value);
    integer work(row_sums[toward]);
    work.multiply(dimension_value);
    work.multiply(2);
    curvature_numerator -= work;
    curvature_numerator += total;

    // A nonpositive curvature has its minimum at an endpoint. The centre and every vertex have already passed the selected mode.
    // The same is true when the unconstrained minimizer lies at or beyond the toward vertex.
    if (curvature_numerator.sign() <= 0 || descent_numerator.compare(curvature_numerator) >= 0) return false;

#ifdef COPOSIT_FRANK_WOLFE_SPONSEL_TESTING
    last_frank_wolfe_line_step = true;
#endif

    // alpha = descent_numerator / curvature_numerator. Multiplying the rational point by n*curvature_numerator gives integer
    // weights z = (q-p)*1 + n*p*e_j, so homogeneity permits one exact quadratic evaluation without storing z.
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
    value.addmul(coefficient, gram(toward, toward));

    if (!fails_mode(value, mode)) return false;
#ifdef COPOSIT_FRANK_WOLFE_SPONSEL_TESTING
    last_frank_wolfe_witness = true;
#endif
    return true;
}

evaluation inspect(const matrix_integer& gram, fraction_free_ldlt_factorization& factorization, copositivity_mode mode)
{
    const size_t dimension = gram.rows();
    for (size_t i = 0; i < dimension; ++i) {
        if (fails_mode(gram(i, i), mode)) return {state::reject, dimension, dimension};
    }

    size_t split_i = dimension;
    size_t split_j = dimension;
    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        for (size_t j = i + 1; j < dimension; ++j) {
            const integer::const_reference entry = gram(i, j);
            if (entry.sign() < 0 && (split_i == dimension || entry.compare(gram(split_i, split_j)) < 0)) {
                split_i = i;
                split_j = j;
            }
        }
    }

    if (split_i == dimension) return {state::accept, dimension, dimension};

    integer diagonal_product;
    integer edge_squared;
    diagonal_product.set_product(gram(split_i, split_i), gram(split_j, split_j));
    edge_squared.set_product(gram(split_i, split_j), gram(split_i, split_j));
    const int edge_comparison = edge_squared.compare(diagonal_product);
    if (edge_comparison > 0 || (edge_comparison == 0 && mode == copositivity_mode::strictly_copositive)) {
        return {state::reject, dimension, dimension};
    }

    if (passes_h_certificate(gram, factorization, mode)) return {state::accept, dimension, dimension};
    if (has_exact_one_step_frank_wolfe_witness(gram, mode)) return {state::reject, dimension, dimension};
    return {state::split, split_i, split_j};
}

void reduce(positive_ratio& ratio)
{
    integer divisor;
    fmpz_gcd(divisor.native_handle(), ratio.numerator.native_handle(), ratio.denominator.native_handle());
    ratio.numerator.divide_exact(divisor);
    ratio.denominator.divide_exact(divisor);
}

positive_ratio calculate_lambda(integer::const_reference alpha, integer::const_reference beta,
                                integer::const_reference gamma)
{
    positive_ratio lambda1;
    positive_ratio lambda2;
    positive_ratio lambda3;

    lambda1.numerator.set_abs(gamma);
    lambda1.denominator.set_difference(alpha, gamma);

    lambda3.numerator = beta;
    lambda3.denominator.set_difference(beta, gamma);

    lambda2.numerator = lambda3.denominator;
    lambda2.denominator = alpha;
    lambda2.denominator -= gamma;
    lambda2.denominator -= gamma;
    lambda2.denominator += beta;

    positive_ratio result = compare(lambda2, lambda3) < 0 ? lambda2 : lambda3;
    if (compare(result, lambda1) < 0) result = lambda1;
    reduce(result);
    return result;
}

void divide_by_content(matrix_integer& gram)
{
    integer content;
    integer next;
    const size_t dimension = gram.rows();
    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        for (size_t j = i; j < dimension; ++j) {
            fmpz_gcd(next.native_handle(), content.native_handle(), gram(i, j).native_handle());
            content.swap(next);
            if (content.is_one()) return;
        }
    }
    fmpz_mat_scalar_divexact_fmpz(gram.native_handle(), gram.native_handle(), content.native_handle());
}

std::pair<matrix_integer, matrix_integer> split(const matrix_integer& gram, size_t first, size_t second)
{
    const positive_ratio lambda = calculate_lambda(gram(first, first), gram(second, second), gram(first, second));
    integer complement;
    complement.set_difference(lambda.denominator, lambda.numerator);

    integer denominator_squared;
    denominator_squared.set_product(lambda.denominator, lambda.denominator);

    const size_t dimension = gram.rows();
    std::vector<integer> new_row(dimension);
    for (size_t k = 0; k < dimension; ++k) {
        new_row[k].set_product(lambda.numerator, gram(first, k));
        new_row[k].addmul(complement, gram(second, k));
        fmpz_mul(new_row[k].native_handle(), new_row[k].native_handle(), lambda.denominator.native_handle());
    }

    integer new_diagonal;
    integer coefficient;
    integer work;
    coefficient.set_product(lambda.numerator, lambda.numerator);
    new_diagonal.set_product(coefficient, gram(first, first));
    coefficient.set_product(lambda.numerator, complement);
    work.set_product(coefficient, gram(first, second));
    work.multiply(2);
    new_diagonal += work;
    coefficient.set_product(complement, complement);
    new_diagonal.addmul(coefficient, gram(second, second));

    matrix_integer first_child(gram);
    matrix_integer second_child(gram);
    fmpz_mat_scalar_mul_fmpz(first_child.native_handle(), first_child.native_handle(), denominator_squared.native_handle());
    fmpz_mat_scalar_mul_fmpz(second_child.native_handle(), second_child.native_handle(), denominator_squared.native_handle());

    for (size_t k = 0; k < dimension; ++k) {
        if (k != first) {
            first_child(first, k) = new_row[k];
            first_child(k, first) = new_row[k];
        }
        if (k != second) {
            second_child(second, k) = new_row[k];
            second_child(k, second) = new_row[k];
        }
    }
    first_child(first, first) = new_diagonal;
    second_child(second, second) = new_diagonal;
    divide_by_content(first_child);
    divide_by_content(second_child);
    return {std::move(first_child), std::move(second_child)};
}

bool test_copositivity(const matrix_integer& matrix, copositivity_mode mode)
{
    fraction_free_ldlt_factorization h_factorization(matrix.rows());
    const evaluation initial = inspect(matrix, h_factorization, mode);
    if (initial.result == state::reject) return false;
    if (initial.result == state::accept) return true;

    std::vector<node> pending;
    pending.reserve(64); // Initial capacity only; this is not a dimension limit.
    pending.push_back({matrix_integer(matrix), initial.split_i, initial.split_j});

    while (!pending.empty()) {
        timeout_checkpoint();
        node current = std::move(pending.back());
        pending.pop_back();
        enforce_open_node_limit(pending.size() + 2);
        auto children = split(current.gram, current.split_i, current.split_j);

        evaluation child = inspect(children.first, h_factorization, mode);
        if (child.result == state::reject) return false;
        if (child.result == state::split) {
            pending.push_back({std::move(children.first), child.split_i, child.split_j});
        }

        child = inspect(children.second, h_factorization, mode);
        if (child.result == state::reject) return false;
        if (child.result == state::split) {
            pending.push_back({std::move(children.second), child.split_i, child.split_j});
        }
    }

    return true;
}

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
#ifdef COPOSIT_FRANK_WOLFE_SPONSEL_TESTING
    last_frank_wolfe_witness = false;
    last_frank_wolfe_line_step = false;
#endif
    timeout_checkpoint();
    return test_copositivity(matrix, mode);
}

#ifdef COPOSIT_FRANK_WOLFE_SPONSEL_TESTING
bool frank_wolfe_witness_found_for_testing() noexcept
{
    return last_frank_wolfe_witness;
}

bool frank_wolfe_line_step_used_for_testing() noexcept
{
    return last_frank_wolfe_line_step;
}
#endif

} // namespace coposit::model
