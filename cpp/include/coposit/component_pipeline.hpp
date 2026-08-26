#pragma once

#include <coposit/connected_components.hpp>
#include <coposit/copomatrix_precheck.hpp>
#include <coposit/danninger_precheck.hpp>
#include <coposit/matrix_scan.hpp>
#include <coposit/pre_check.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace coposit::component_pipeline {

struct options {
    bool preprocessing_enabled = true;
};

namespace detail {

using outcome = copomatrix_precheck::outcome;
using query = pre_check::detail::query;
using state = pre_check::detail::classification_state;
inline constexpr size_t maximum_reduction_depth = 2;

struct component_result {
    component_result(state partial_result) : partial_result(partial_result) {}
    component_result(state partial_result, const matrix_integer& matrix) : partial_result(partial_result), borrowed_matrix(&matrix) {}
    component_result(state partial_result, matrix_integer&& matrix)
        : partial_result(partial_result), owned_matrix(std::make_unique<matrix_integer>(std::move(matrix)))
    {
    }

    const matrix_integer& matrix() const noexcept
    {
        assert(owned_matrix || borrowed_matrix);
        return owned_matrix ? *owned_matrix : *borrowed_matrix;
    }

    bool has_matrix() const noexcept
    {
        return owned_matrix || borrowed_matrix;
    }

    state partial_result;
    // A connected input borrows its caller-owned matrix; a proper component owns
    // its one materialized dense block.
    std::unique_ptr<matrix_integer> owned_matrix;
    const matrix_integer* borrowed_matrix = nullptr;
};

struct preprocessing_result {
    state aggregate() const
    {
        if (components.empty()) return root_result;
        state combined_components;
        combined_components.accept_strict();
        for (const auto& component : components)
            combined_components.combine_by_and(component.partial_result);
        state result = root_result;
        result.merge(combined_components);
        return result;
    }

    state root_result;
    std::vector<component_result> components;
};

template<query requested>
bool should_stop(const state& result) noexcept
{
    if constexpr (requested == query::copositive) return result.copositive_known && !result.value.is_copositive;
    if constexpr (requested == query::strict) return result.strict_known && !result.value.is_strictly_copositive;
    return result.copositive_known && !result.value.is_copositive;
}

template<query requested>
outcome selected_outcome(const state& result) noexcept
{
    static_assert(requested != query::combined);
    if constexpr (requested == query::copositive) {
        if (!result.copositive_known) return outcome::unresolved;
        return result.value.is_copositive ? outcome::accepted : outcome::rejected;
    } else {
        if (!result.strict_known) return outcome::unresolved;
        return result.value.is_strictly_copositive ? outcome::accepted : outcome::rejected;
    }
}

template<query requested>
preprocessing_result preprocess(const matrix_integer& matrix, size_t reduction_depth, size_t maximum_depth);

template<query requested>
outcome run_reductions(const matrix_integer& matrix, const matrix_scan_result& scan, size_t reduction_depth, size_t maximum_depth)
{
    static_assert(requested != query::combined);
    const model::copositivity_mode mode =
        requested == query::strict ? model::copositivity_mode::strictly_copositive : model::copositivity_mode::copositive;
    auto preprocess_child = [&](const matrix_integer& child) {
        return selected_outcome<requested>(preprocess<requested>(child, reduction_depth + 1, maximum_depth).aggregate());
    };

    const outcome danninger = danninger_precheck::check(matrix, scan, mode, preprocess_child);
    if (danninger != outcome::unresolved) return danninger;
    return copomatrix_precheck::check(matrix, scan, mode, preprocess_child);
}

template<query requested>
void observe_reduction(state& result, outcome reduced) noexcept
{
    static_assert(requested != query::combined);
    if (reduced == outcome::unresolved) return;
    diagnostics::preprocessing_reduction_decision();
    if constexpr (requested == query::copositive) {
        if (reduced == outcome::accepted)
            result.accept_copositive();
        else
            result.reject_copositive();
    } else {
        if (reduced == outcome::accepted)
            result.accept_strict();
        else
            result.reject_strict();
    }
}

template<query requested>
state check_component(const matrix_integer& matrix, support_context& support_context, const matrix_scan_result& scan,
                      size_t reduction_depth, size_t maximum_depth)
{
    state result = pre_check::detail::ordinary_checks_scanned<requested>(matrix, support_context, scan);
    if (result.done<requested>() || reduction_depth >= maximum_depth) return result;

    if constexpr (requested == query::combined) {
        if (!result.strict_known)
            observe_reduction<query::strict>(result, run_reductions<query::strict>(matrix, scan, reduction_depth, maximum_depth));
        if (!result.copositive_known)
            observe_reduction<query::copositive>(result, run_reductions<query::copositive>(matrix, scan, reduction_depth, maximum_depth));
    } else {
        observe_reduction<requested>(result, run_reductions<requested>(matrix, scan, reduction_depth, maximum_depth));
    }
    return result;
}

template<query requested>
preprocessing_result preprocess(const matrix_integer& matrix, size_t reduction_depth, size_t maximum_depth)
{
    if (reduction_depth == 0)
        diagnostics::preprocessing_begin(matrix.rows());
    else
        diagnostics::preprocessing_reduction_child(reduction_depth);
    preprocessing_result output;
    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::matrix_scan, matrix.rows(), 0, matrix.rows());
    const matrix_scan_requirements requirements = pre_check::detail::preprocessing_requirements<requested>();
    support_context root_support_context(matrix.rows());
    const matrix_scan_result root_scan = scan_matrix(matrix, requirements, root_support_context);
    output.root_result = pre_check::detail::root_checks_scanned<requested>(matrix, root_scan);
    if (output.root_result.done<requested>()) {
        if (reduction_depth == 0) diagnostics::preprocessing_complete(0, 0);
        return output;
    }

    state components;
    components.accept_strict();
    std::vector<size_t> indices;
    indices.reserve(matrix.rows());
    size_t component_number = 0;

    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::connected_components, matrix.rows(), 0, matrix.rows());
    connected_components::visit(root_support_context, root_scan.negative_neighbors, [&](const support& component, bool is_whole_graph) {
        ++component_number;
        state current;
        if (is_whole_graph) {
            if (reduction_depth == 0) diagnostics::preprocessing_top_component(matrix.rows(), false);
            current = check_component<requested>(matrix, root_support_context, root_scan, reduction_depth, maximum_depth);
            if (current.done<requested>())
                output.components.emplace_back(current);
            else
                output.components.emplace_back(current, matrix);
        } else {
            root_support_context.extract_set_indices(component, indices);
            if (reduction_depth == 0) diagnostics::preprocessing_top_component(indices.size(), true);
            diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::component_scan, indices.size(), component_number);
            support_context part_support_context(indices.size());
            scanned_principal_matrix part = scan_principal_matrix(matrix, indices, requirements, part_support_context);
            current = check_component<requested>(part.matrix, part_support_context, part.scan, reduction_depth, maximum_depth);
            if (current.done<requested>())
                output.components.emplace_back(current);
            else
                output.components.emplace_back(current, std::move(part.matrix));
        }
        components.combine_by_and(current);
        return !should_stop<requested>(components);
    });
    const state result = output.aggregate();
    if (result.done<requested>()) {
        output.root_result = result;
        output.components.clear();
    }
    if (reduction_depth == 0) {
        size_t pending_components = 0;
        size_t largest_pending_component = 0;
        for (const component_result& component : output.components) {
            if (!component.has_matrix()) continue;
            ++pending_components;
            largest_pending_component = std::max(largest_pending_component, component.matrix().rows());
        }
        diagnostics::preprocessing_complete(pending_components, largest_pending_component);
    }
    return output;
}

} // namespace detail

/* Fixed exact preprocessing followed by the selected model only for unresolved
 * component properties. */
template<typename FinalAlgorithm>
bool check(const matrix_integer& matrix, model::copositivity_mode mode, const options& selected, FinalAlgorithm&& final_algorithm)
{
    if (!selected.preprocessing_enabled) return final_algorithm(matrix);
    if (mode == model::copositivity_mode::strictly_copositive) {
        detail::preprocessing_result preprocessing = detail::preprocess<detail::query::strict>(matrix, 0, detail::maximum_reduction_depth);
        detail::state result = preprocessing.aggregate();
        if (result.strict_known) return result.value.is_strictly_copositive;
        for (auto& component : preprocessing.components) {
            if (component.partial_result.strict_known) continue;
            diagnostics::preprocessing_model_delegation();
            diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::model_delegation, component.matrix().rows());
            if (final_algorithm(component.matrix()))
                component.partial_result.accept_strict();
            else
                return false;
        }
        result = preprocessing.aggregate();
        assert(result.strict_known);
        return result.value.is_strictly_copositive;
    } else {
        detail::preprocessing_result preprocessing =
            detail::preprocess<detail::query::copositive>(matrix, 0, detail::maximum_reduction_depth);
        detail::state result = preprocessing.aggregate();
        if (result.copositive_known) return result.value.is_copositive;
        for (auto& component : preprocessing.components) {
            if (component.partial_result.copositive_known) continue;
            diagnostics::preprocessing_model_delegation();
            diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::model_delegation, component.matrix().rows());
            if (final_algorithm(component.matrix()))
                component.partial_result.accept_copositive();
            else
                return false;
        }
        result = preprocessing.aggregate();
        assert(result.copositive_known);
        return result.value.is_copositive;
    }
}

template<typename FinalClassifier>
model::copositivity_classification classify(const matrix_integer& matrix, const options& selected, FinalClassifier&& final_classifier)
{
    if (!selected.preprocessing_enabled) return final_classifier(matrix);
    detail::preprocessing_result preprocessing = detail::preprocess<detail::query::combined>(matrix, 0, detail::maximum_reduction_depth);
    detail::state result = preprocessing.aggregate();
    if (result.done<detail::query::combined>()) return result.value;
    for (auto& component : preprocessing.components) {
        if (component.partial_result.done<detail::query::combined>()) continue;
        diagnostics::preprocessing_model_delegation();
        diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::model_delegation, component.matrix().rows());
        const model::copositivity_classification component_classification = final_classifier(component.matrix());
        if (!component_classification.is_copositive) return {false, false};
        component.partial_result.merge(component_classification);
    }
    result = preprocessing.aggregate();
    assert(result.done<detail::query::combined>());
    return result.value;
}

} // namespace coposit::component_pipeline
