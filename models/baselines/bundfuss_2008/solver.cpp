#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include "../source_diagnostics.hpp"

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

evaluation inspect(const matrix_integer& gram, copositivity_mode mode)
{
    const size_t dimension = gram.rows();
    for (size_t i = 0; i < dimension; ++i) {
        if (gram(i, i).sign() < (mode == copositivity_mode::copositive ? 0 : 1)) {
            COPOSIT_SOURCE_DIAGNOSTICS("reject");
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
        COPOSIT_SOURCE_DIAGNOSTICS("accept");
        return {state::accept, dimension, dimension};
    }

    integer diagonal_product;
    integer edge_squared;
    diagonal_product.set_product(gram(split_i, split_i), gram(split_j, split_j));
    edge_squared.set_product(gram(split_i, split_j), gram(split_i, split_j));
    const int edge_comparison = edge_squared.compare(diagonal_product);
    if (edge_comparison > 0 || (edge_comparison == 0 && mode == copositivity_mode::strictly_copositive)) {
        COPOSIT_SOURCE_DIAGNOSTICS("reject");
        return {state::reject, dimension, dimension};
    }

    COPOSIT_SOURCE_DIAGNOSTICS("split", split_i, split_j);
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

split_data prepare_split(const matrix_integer& gram, size_t first, size_t second, bool track_diagnostics)
{
    const positive_ratio lambda = calculate_lambda(gram(first, first), gram(second, second), gram(first, second));
    COPOSIT_SOURCE_DIAGNOSTICS("lambda", fmpz_get_ui(lambda.numerator.native_handle()), fmpz_get_ui(lambda.denominator.native_handle()));
    integer complement;
    complement.set_difference(lambda.denominator, lambda.numerator);

    split_data result;
    if (track_diagnostics) {
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
 * Exact non-strict implementation and strict adaptation of Bundfuss and Dür's 2008 simplicial partition as realized by the preserved 2018
 * implementation. It retains the minimum edge, three lambda formulas, two child simplices, child evaluation order, and LIFO
 * depth-first traversal. Each node stores a positive integer multiple of its rational Gram matrix; clearing a common denominator
 * changes no sign, comparison, split parameter, or mathematical decision.
 */
bool test_copositivity(const matrix_integer& matrix, copositivity_mode mode)
{
    diagnostics::tracker diagnostics(diagnostics::metric::simplex, matrix.rows());
    const evaluation initial = inspect(matrix, mode);
    diagnostics.visit(matrix.rows(), 0, 1);
    if (initial.result == state::reject) {
        diagnostics.finish();
        return false;
    }
    if (initial.result == state::accept) {
        diagnostics.resolved(diagnostics.active() ? 1.0L : 0.0L);
        diagnostics.finish();
        return true;
    }
    diagnostics.split();

    std::vector<node> pending;
    pending.reserve(64); // Initial capacity only; this is not a dimension limit.
    pending.push_back({matrix_integer(matrix), initial.split_i, initial.split_j, diagnostics.active() ? 1.0L : 0.0L, 0});

    while (!pending.empty()) {
        timeout_checkpoint();
        node current = std::move(pending.back());
        pending.pop_back();
        enforce_open_node_limit(pending.size() + 2);
        const split_data split = prepare_split(current.gram, current.split_i, current.split_j, diagnostics.active());

        matrix_integer child_gram = make_child(current.gram, split, current.split_i);
        evaluation child = inspect(child_gram, mode);
        const long double first_weight = current.weight * split.lambda;
        diagnostics.visit(matrix.rows(), current.depth + 1, pending.size() + 2);
        if (child.result == state::reject) {
            diagnostics.finish();
            return false;
        }
        if (child.result == state::accept) diagnostics.resolved(first_weight);
        if (child.result == state::split) {
            diagnostics.split();
            pending.push_back({std::move(child_gram), child.split_i, child.split_j, first_weight, current.depth + 1});
        }

        child_gram = make_child(current.gram, split, current.split_j);
        child = inspect(child_gram, mode);
        const long double second_weight = diagnostics.active() ? current.weight - first_weight : 0.0L;
        diagnostics.visit(matrix.rows(), current.depth + 1, pending.size() + 1);
        if (child.result == state::reject) {
            diagnostics.finish();
            return false;
        }
        if (child.result == state::accept) diagnostics.resolved(second_weight);
        if (child.result == state::split) {
            diagnostics.split();
            pending.push_back({std::move(child_gram), child.split_i, child.split_j, second_weight, current.depth + 1});
        }
    }

    diagnostics.finish();
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
