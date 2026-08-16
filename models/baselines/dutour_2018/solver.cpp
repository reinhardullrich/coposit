#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include "../source_diagnostics.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

enum class inspection { reject, accept, split };

struct node {
    matrix_integer gram;
    long double weight;
    size_t depth;
};

inspection inspect(const matrix_integer& gram, size_t& split_i, size_t& split_j, copositivity_mode mode)
{
    const size_t dimension = gram.rows();
    for (size_t i = 0; i < dimension; ++i) {
        if (gram(i, i).sign() < (mode == copositivity_mode::copositive ? 0 : 1)) return inspection::reject;
    }

    split_i = dimension;
    split_j = dimension;
    integer best_numerator;
    integer best_denominator;
    integer numerator;
    integer denominator;
    integer left;
    integer right;

    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        for (size_t j = i + 1; j < dimension; ++j) {
            const integer::const_reference entry = gram(i, j);
            if (entry.sign() >= 0) continue;

            numerator.set_product(entry, entry);
            denominator.set_product(gram(i, i), gram(j, j));

            // Dutour Sikirić rejects equality as a zero direction and a larger numerator as a negative direction.
            const int edge_comparison = numerator.compare(denominator);
            if (edge_comparison > 0 || (edge_comparison == 0 && mode == copositivity_mode::strictly_copositive)) {
                return inspection::reject;
            }

            bool take_pair = split_i == dimension;
            if (!take_pair) {
                left.set_product(numerator, best_denominator);
                right.set_product(best_numerator, denominator);
                take_pair = left.compare(right) > 0;
            }
            if (take_pair) {
                split_i = i;
                split_j = j;
                best_numerator = numerator;
                best_denominator = denominator;
            }
        }
    }

    return split_i == dimension ? inspection::accept : inspection::split;
}

void replace_generator_with_sum(matrix_integer& gram, size_t replaced, size_t other)
{
    integer new_diagonal(gram(replaced, replaced));
    new_diagonal += gram(replaced, other);
    new_diagonal += gram(replaced, other);
    new_diagonal += gram(other, other);

    const size_t dimension = gram.rows();
    for (size_t k = 0; k < dimension; ++k) {
        if (k == replaced) continue;
        integer sum(gram(replaced, k));
        sum += gram(other, k);
        gram(replaced, k) = sum;
        gram(k, replaced) = sum;
    }
    gram(replaced, replaced) = new_diagonal;
}

/*
 * Faithful Boolean adaptation of Mathieu Dutour Sikirić's 2018 copositivity traversals and PairDecomposition. The mathematical
 * tests, maximum-ratio split, midpoint generator, two child cones, and depth-first traversal are unchanged. Each node stores only
 * B = V A V^T, so replacing one generator by v_i + v_j needs one exact row-and-column update instead of rebuilding B from V.
 */
bool test_copositivity(const matrix_integer& matrix, copositivity_mode mode)
{
    diagnostics::tracker diagnostics(diagnostics::metric::proof, matrix.rows());
    std::vector<node> pending;
    pending.reserve(64); // Initial capacity only; this is not a dimension limit.
    pending.push_back({matrix_integer(matrix), diagnostics.active() ? 1.0L : 0.0L, 0});

    while (!pending.empty()) {
        timeout_checkpoint();
        node current(std::move(pending.back()));
        pending.pop_back();
        diagnostics.visit(matrix.rows(), current.depth, pending.size() + 1);

        size_t split_i;
        size_t split_j;
        switch (inspect(current.gram, split_i, split_j, mode)) {
            case inspection::reject:
                COPOSIT_SOURCE_DIAGNOSTICS("reject");
                diagnostics.finish();
                return false;
            case inspection::accept:
                COPOSIT_SOURCE_DIAGNOSTICS("accept");
                diagnostics.resolved(current.weight);
                continue;
            case inspection::split:
                COPOSIT_SOURCE_DIAGNOSTICS("split", split_i, split_j);
                diagnostics.split();
                break;
        }

        enforce_open_node_limit(pending.size() + 2);
        matrix_integer second_child(current.gram);
        replace_generator_with_sum(current.gram, split_i, split_j);
        replace_generator_with_sum(second_child, split_j, split_i);
        const long double child_weight = current.weight * 0.5L;

        // LIFO order visits Dutour Sikirić's first child before its sibling.
        COPOSIT_SOURCE_DIAGNOSTICS("push-second", split_j, split_i);
        pending.push_back({std::move(second_child), child_weight, current.depth + 1});
        COPOSIT_SOURCE_DIAGNOSTICS("push-first", split_i, split_j);
        pending.push_back({std::move(current.gram), child_weight, current.depth + 1});
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
