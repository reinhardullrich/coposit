#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/progress.hpp>
#include <coposit/timeout.hpp>

#include "../source_trace.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

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

struct split_data {
    integer denominator_squared;
    std::vector<integer> new_row;
    integer new_diagonal;
    long double lambda = 0.0L;
};

struct node {
    matrix_integer gram;
    size_t split_i;
    size_t split_j;
    long double weight;
    size_t depth;
};

int compare(const positive_ratio& left, const positive_ratio& right)
{
    integer cross_left;
    integer cross_right;
    cross_left.set_product(left.numerator, right.denominator);
    cross_right.set_product(right.numerator, left.denominator);
    return cross_left.compare(cross_right);
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

evaluation inspect(const matrix_integer& gram, fraction_free_ldlt_factorization& factorization, copositivity_mode mode)
{
    const size_t dimension = gram.rows();
    for (size_t i = 0; i < dimension; ++i) {
        if (gram(i, i).sign() < (mode == copositivity_mode::copositive ? 0 : 1)) {
            COPOSIT_SOURCE_TRACE("reject");
            return {state::reject, dimension, dimension};
        }
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

    if (split_i == dimension) {
        COPOSIT_SOURCE_TRACE("entrywise-accept");
        return {state::accept, dimension, dimension};
    }

    integer diagonal_product;
    integer edge_squared;
    diagonal_product.set_product(gram(split_i, split_i), gram(split_j, split_j));
    edge_squared.set_product(gram(split_i, split_j), gram(split_i, split_j));
    const int edge_comparison = edge_squared.compare(diagonal_product);
    if (edge_comparison > 0 || (edge_comparison == 0 && mode == copositivity_mode::strictly_copositive)) {
        COPOSIT_SOURCE_TRACE("reject");
        return {state::reject, dimension, dimension};
    }

    if (passes_h_certificate(gram, factorization, mode)) {
        COPOSIT_SOURCE_TRACE("h-accept");
        return {state::accept, dimension, dimension};
    }
    COPOSIT_SOURCE_TRACE("split", split_i, split_j);
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

split_data prepare_split(const matrix_integer& gram, size_t first, size_t second, bool track_progress)
{
    const positive_ratio lambda = calculate_lambda(gram(first, first), gram(second, second), gram(first, second));
    integer complement;
    complement.set_difference(lambda.denominator, lambda.numerator);

    split_data result;
    if (track_progress) {
        slong numerator_exponent;
        slong denominator_exponent;
        const long double numerator = lambda.numerator.to_dbl_2exp(numerator_exponent);
        const long double denominator = lambda.denominator.to_dbl_2exp(denominator_exponent);
        result.lambda = std::ldexp(numerator / denominator, static_cast<int>(numerator_exponent - denominator_exponent));
    }
    result.denominator_squared.set_product(lambda.denominator, lambda.denominator);

    const size_t dimension = gram.rows();
    result.new_row.resize(dimension);
    for (size_t k = 0; k < dimension; ++k) {
        result.new_row[k].set_product(lambda.numerator, gram(first, k));
        result.new_row[k].addmul(complement, gram(second, k));
        fmpz_mul(result.new_row[k].native_handle(), result.new_row[k].native_handle(), lambda.denominator.native_handle());
    }

    integer coefficient;
    integer work;
    coefficient.set_product(lambda.numerator, lambda.numerator);
    result.new_diagonal.set_product(coefficient, gram(first, first));
    coefficient.set_product(lambda.numerator, complement);
    work.set_product(coefficient, gram(first, second));
    work.multiply(2);
    result.new_diagonal += work;
    coefficient.set_product(complement, complement);
    result.new_diagonal.addmul(coefficient, gram(second, second));
    return result;
}

matrix_integer make_child(const matrix_integer& gram, const split_data& split, size_t replaced)
{
    matrix_integer child(gram);
    fmpz_mat_scalar_mul_fmpz(child.native_handle(), child.native_handle(), split.denominator_squared.native_handle());

    const size_t dimension = gram.rows();
    for (size_t k = 0; k < dimension; ++k) {
        if (k == replaced) continue;
        child(replaced, k) = split.new_row[k];
        child(k, replaced) = split.new_row[k];
    }
    child(replaced, replaced) = split.new_diagonal;
    divide_by_content(child);
    return child;
}

/*
 * Non-strict implementation and strict adaptation of Sponsel, Bundfuss, and Dür's 2012 H-enhanced simplicial-partition framework.
 * A node first uses the cheap entrywise-nonnegative certificate, then tests S(G) >= 0 or S(G) > 0 according to the mode, where
 * S(G) removes positive off-diagonal entries. Unresolved nodes retain the exact minimum-edge split, lambda rule, and LIFO traversal
 * of bundfuss_2008.
 */
bool test_copositivity(const matrix_integer& matrix, copositivity_mode mode)
{
    progress::tracker progress(progress::metric::simplex, matrix.rows());
    fraction_free_ldlt_factorization h_factorization(matrix.rows());
    const evaluation initial = inspect(matrix, h_factorization, mode);
    progress.visit(matrix.rows(), 0, 1);
    if (initial.result == state::reject) {
        progress.finish();
        return false;
    }
    if (initial.result == state::accept) {
        progress.resolved(progress.active() ? 1.0L : 0.0L);
        progress.finish();
        return true;
    }
    progress.split();

    std::vector<node> pending;
    pending.reserve(64); // Initial capacity only; this is not a dimension limit.
    pending.push_back({matrix_integer(matrix), initial.split_i, initial.split_j, progress.active() ? 1.0L : 0.0L, 0});

    while (!pending.empty()) {
        timeout_checkpoint();
        node current = std::move(pending.back());
        pending.pop_back();
        enforce_open_node_limit(pending.size() + 2);
        const split_data split = prepare_split(current.gram, current.split_i, current.split_j, progress.active());

        matrix_integer child_gram = make_child(current.gram, split, current.split_i);
        evaluation child = inspect(child_gram, h_factorization, mode);
        const long double first_weight = current.weight * split.lambda;
        progress.visit(matrix.rows(), current.depth + 1, pending.size() + 2);
        if (child.result == state::reject) {
            progress.finish();
            return false;
        }
        if (child.result == state::accept) progress.resolved(first_weight);
        if (child.result == state::split) {
            progress.split();
            pending.push_back({std::move(child_gram), child.split_i, child.split_j, first_weight, current.depth + 1});
        }

        child_gram = make_child(current.gram, split, current.split_j);
        child = inspect(child_gram, h_factorization, mode);
        const long double second_weight = progress.active() ? current.weight - first_weight : 0.0L;
        progress.visit(matrix.rows(), current.depth + 1, pending.size() + 1);
        if (child.result == state::reject) {
            progress.finish();
            return false;
        }
        if (child.result == state::accept) progress.resolved(second_weight);
        if (child.result == state::split) {
            progress.split();
            pending.push_back({std::move(child_gram), child.split_i, child.split_j, second_weight, current.depth + 1});
        }
    }

    progress.finish();
    return true;
}

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    return test_copositivity(matrix, mode);
}

} // namespace coposit::model
