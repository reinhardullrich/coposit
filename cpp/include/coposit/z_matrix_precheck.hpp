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

inline outcome check_motzkin_straus(
    const support_context& support_context, const matrix_scan_result& scan, request requested)
{
    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::motzkin_straus, scan.dimension);
    open_mcs::maximum_clique_search search(support_context, scan.negative_neighbors);
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
    maximal_z_blocks(support_context& support_context, const std::vector<support>& adjacency)
        : support_context_(support_context)
        , adjacency_(adjacency)
    {
        const size_t stack_size = adjacency.size() + 1;
        candidates_.reserve(stack_size);
        excluded_.reserve(stack_size);
        extensions_.reserve(stack_size);
        for (size_t depth = 0; depth < stack_size; ++depth) {
            candidates_.push_back(support_context_.make());
            excluded_.push_back(support_context_.make());
            extensions_.push_back(support_context_.make());
        }
        vertices_.resize(stack_size);
    }

    ~maximal_z_blocks()
    {
        for (support& value : candidates_) support_context_.release(std::move(value));
        for (support& value : excluded_) support_context_.release(std::move(value));
        for (support& value : extensions_) support_context_.release(std::move(value));
    }

    template<typename Visitor>
    bool visit(Visitor&& visitor)
    {
        support block = support_context_.make();
        support_context_.set_all(candidates_[0]);
        support_context_.clear(excluded_[0]);
        const bool result = search(block, 0, visitor);
        support_context_.release(std::move(block));
        return result;
    }

private:
    template<typename Visitor>
    bool search(support& block, size_t depth, Visitor& visitor)
    {
        timeout_checkpoint();
        support& candidates = candidates_[depth];
        support& excluded = excluded_[depth];
        if (support_context_.empty(candidates) && support_context_.empty(excluded)) return visitor(block);

        const size_t pivot = !support_context_.empty(candidates) ? support_context_.first(candidates) : support_context_.first(excluded);
        support& extensions = extensions_[depth];
        support_context_.copy(extensions, candidates);
        support_context_.subtract(extensions, adjacency_[pivot]);
        support_context_.extract_set_indices(extensions, vertices_[depth]);

        for (const size_t vertex : vertices_[depth]) {
            support& child_candidates = candidates_[depth + 1];
            support& child_excluded = excluded_[depth + 1];
            support_context_.copy(child_candidates, candidates);
            support_context_.copy(child_excluded, excluded);
            support_context_.intersect(child_candidates, adjacency_[vertex]);
            support_context_.intersect(child_excluded, adjacency_[vertex]);

            support_context_.set(block, vertex);
            if (!search(block, depth + 1, visitor)) return false;
            support_context_.reset(block, vertex);
            support_context_.reset(candidates, vertex);
            support_context_.set(excluded, vertex);
        }
        return true;
    }

    support_context& support_context_;
    const std::vector<support>& adjacency_;
    std::vector<support> candidates_;
    std::vector<support> excluded_;
    std::vector<support> extensions_;
    std::vector<std::vector<size_t>> vertices_;
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
inline outcome check(
    const matrix_integer& matrix, support_context& support_context, const matrix_scan_result& scan, request requested)
{
    if (matrix.rows() == 0) return outcome::unresolved;
    if (scan.is_motzkin_straus_pattern) return detail::check_motzkin_straus(support_context, scan, requested);

    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::z_matrix, matrix.rows(), 0, matrix.rows());
    detail::maximal_z_blocks blocks(support_context, scan.nonpositive_neighbors);

    fraction_free_ldlt_factorization factorization(matrix.rows());
    matrix_integer principal;
    std::vector<size_t> indices;
    indices.reserve(matrix.rows());
    outcome result = outcome::unresolved;
    size_t block_count = 0;
    support remaining = support_context.make();
    support component = support_context.make();
    support frontier = support_context.make();
    support discovered = support_context.make();

    blocks.visit([&](const support& block) {
        diagnostics::advance_preprocessing(++block_count, 0);
        support_context.copy(remaining, block);

        while (!support_context.empty(remaining)) {
            timeout_checkpoint();
            support_context.clear(component);
            support_context.clear(frontier);
            const size_t first = support_context.first(remaining);
            support_context.set(component, first);
            support_context.set(frontier, first);

            while (!support_context.empty(frontier)) {
                timeout_checkpoint();
                const size_t vertex = support_context.first(frontier);
                support_context.reset(frontier, vertex);
                support_context.copy(discovered, scan.negative_neighbors[vertex]);
                support_context.intersect(discovered, block);
                support_context.subtract(discovered, component);
                support_context.add(component, discovered);
                support_context.add(frontier, discovered);
            }
            support_context.subtract(remaining, component);

            support_context.extract_set_indices(component, indices);
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
    support_context.release(std::move(remaining));
    support_context.release(std::move(component));
    support_context.release(std::move(frontier));
    support_context.release(std::move(discovered));
    return result;
}

} // namespace coposit::z_matrix_precheck
