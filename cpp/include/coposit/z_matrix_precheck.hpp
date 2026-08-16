#pragma once

#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/matrix_scan.hpp>
#include <coposit/open_mcs.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace coposit::z_matrix_precheck {

enum class request { copositive, strict, combined };
enum class outcome {
    unresolved,
    strictly_copositive,
    copositive_not_strictly_copositive,
    not_strictly_copositive,
    not_copositive,
};

namespace detail {

inline int compare_clique_to_threshold(const matrix_scan_result& scan, size_t clique_size)
{
    integer scaled_edge(scan.motzkin_straus_edge);
    scaled_edge.negate();
    scaled_edge.multiply(static_cast<unsigned long>(clique_size));
    integer threshold;
    threshold.set_difference(scan.motzkin_straus_nonedge, scan.motzkin_straus_edge);
    return scaled_edge.compare(threshold);
}

inline outcome check_motzkin_straus(const matrix_scan_result& scan, request requested)
{
    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::motzkin_straus, scan.dimension);
    open_mcs::maximum_clique_search search(scan.negative_neighbors);
    auto enough_to_decide = [&](size_t clique_size) {
        const int comparison = compare_clique_to_threshold(scan, clique_size);
        return comparison > 0 || (requested == request::strict && comparison == 0);
    };
    const open_mcs::search_result result = search.run(enough_to_decide);
    const int comparison = compare_clique_to_threshold(scan, result.best);
    if (comparison > 0) return outcome::not_copositive;
    if (comparison == 0) {
        return result.complete ? outcome::copositive_not_strictly_copositive : outcome::not_strictly_copositive;
    }
    return outcome::strictly_copositive;
}

class maximal_z_blocks {
public:
    explicit maximal_z_blocks(const std::vector<support>& adjacency) : adjacency_(adjacency) {}

    template<typename Visitor>
    bool visit(Visitor&& visitor) const
    {
        support block(adjacency_.size());
        support candidates(adjacency_.size());
        support excluded(adjacency_.size());
        candidates.set_all();
        return search(block, std::move(candidates), std::move(excluded), visitor);
    }

private:
    template<typename Visitor>
    bool search(support& block, support candidates, support excluded, Visitor& visitor) const
    {
        timeout_checkpoint();
        if (candidates.empty() && excluded.empty()) return visitor(block);

        const size_t pivot = !candidates.empty() ? candidates.lowest_index() : excluded.lowest_index();
        support extensions = candidates;
        extensions.remove(adjacency_[pivot]);
        std::vector<size_t> vertices;
        extensions.copy_indices_to(vertices);

        for (const size_t vertex : vertices) {
            support child_candidates = candidates;
            support child_excluded = excluded;
            child_candidates.intersect_with(adjacency_[vertex]);
            child_excluded.intersect_with(adjacency_[vertex]);

            block.set(vertex);
            if (!search(block, std::move(child_candidates), std::move(child_excluded), visitor)) return false;
            block.reset(vertex);
            candidates.reset(vertex);
            excluded.set(vertex);
        }
        return true;
    }

    const std::vector<support>& adjacency_;
};

inline void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
{
    principal.resize(indices.size(), indices.size());
    for (size_t row = 0; row < indices.size(); ++row) {
        timeout_checkpoint();
        for (size_t column = 0; column < indices.size(); ++column) {
            principal(row, column) = matrix(indices[row], indices[column]);
        }
    }
}

} // namespace detail

/*
 * A recognized Motzkin--Straus matrix receives its complete graph-based classification. Every other matrix follows the maximal
 * principal Z-matrix search: a non-PSD block disproves copositivity and a singular PSD block disproves strict copositivity.
 */
inline outcome check(const matrix_integer& matrix, const matrix_scan_result& scan, request requested)
{
    if (matrix.rows() == 0) return outcome::unresolved;
    if (scan.is_motzkin_straus_pattern) return detail::check_motzkin_straus(scan, requested);

    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::z_matrix, matrix.rows(), 0, matrix.rows());
    const detail::maximal_z_blocks blocks(scan.nonpositive_neighbors);

    fraction_free_ldlt_factorization factorization(matrix.rows());
    matrix_integer principal;
    std::vector<size_t> indices;
    indices.reserve(matrix.rows());
    outcome result = outcome::unresolved;
    size_t block_count = 0;

    blocks.visit([&](const support& block) {
        diagnostics::advance_preprocessing(++block_count, 0);
        support remaining = block;
        support component(matrix.rows());
        support frontier(matrix.rows());
        support discovered(matrix.rows());

        while (!remaining.empty()) {
            timeout_checkpoint();
            component.clear();
            frontier.clear();
            const size_t first = remaining.lowest_index();
            component.set(first);
            frontier.set(first);

            while (!frontier.empty()) {
                timeout_checkpoint();
                const size_t vertex = frontier.lowest_index();
                frontier.reset(vertex);
                discovered = scan.negative_neighbors[vertex];
                discovered.intersect_with(block);
                discovered.remove(component);
                component.add(discovered);
                frontier.add(discovered);
            }
            remaining.remove(component);

            component.copy_indices_to(indices);
            detail::copy_principal(matrix, indices, principal);
            factorization.factorize_inplace(principal);
            if (!factorization.is_positive_semidefinite()) {
                result = outcome::not_copositive;
                return false;
            }
            if (!factorization.is_positive_definite()) {
                result = outcome::not_strictly_copositive;
                if (requested == request::strict) return false;
            }
        }
        return true;
    });
    return result;
}

} // namespace coposit::z_matrix_precheck
