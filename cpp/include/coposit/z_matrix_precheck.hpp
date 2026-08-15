#pragma once

#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/matrix_integer.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace coposit::z_matrix_precheck {

enum class request { copositive, strict, combined };
enum class outcome { unresolved, not_strictly_copositive, not_copositive };

namespace detail {

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
 * Search the maximal principal Z-matrices, meaning blocks whose off-diagonal entries are all nonpositive. A non-PSD block
 * disproves copositivity; a singular PSD block disproves strict copositivity. Passing blocks do not prove either property.
 */
inline outcome check(const matrix_integer& matrix, const std::vector<support>& negative_neighbors,
                     const std::vector<support>& nonpositive_neighbors, bool is_motzkin_straus_pattern, request requested)
{
    if (matrix.rows() == 0) return outcome::unresolved;

    progress::preprocessing_stage(progress::preprocessing_phase::z_matrix, matrix.rows(), 0, matrix.rows());
    if (is_motzkin_straus_pattern) return outcome::unresolved;
    const detail::maximal_z_blocks blocks(nonpositive_neighbors);

    fraction_free_ldlt_factorization factorization(matrix.rows());
    matrix_integer principal;
    std::vector<size_t> indices;
    indices.reserve(matrix.rows());
    outcome result = outcome::unresolved;
    size_t block_count = 0;

    blocks.visit([&](const support& block) {
        progress::advance_preprocessing(++block_count, 0);
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
                discovered = negative_neighbors[vertex];
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
