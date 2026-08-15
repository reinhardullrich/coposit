#pragma once

#include <coposit/connected_components.hpp>
#include <coposit/copomatrix_precheck.hpp>
#include <coposit/danninger_precheck.hpp>
#include <coposit/matrix_scan.hpp>
#include <coposit/pre_check.hpp>

#include <cstddef>
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
state preprocess(const matrix_integer& matrix, size_t reduction_depth, size_t maximum_depth);

template<query requested>
outcome run_reductions(const matrix_integer& matrix, const matrix_scan_result& scan, size_t reduction_depth, size_t maximum_depth)
{
    static_assert(requested != query::combined);
    const model::copositivity_mode mode = requested == query::strict
        ? model::copositivity_mode::strictly_copositive
        : model::copositivity_mode::copositive;
    auto preprocess_child = [&](const matrix_integer& child) {
        return selected_outcome<requested>(preprocess<requested>(child, reduction_depth + 1, maximum_depth));
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
    if constexpr (requested == query::copositive) {
        if (reduced == outcome::accepted) result.accept_copositive();
        else result.reject_copositive();
    } else {
        if (reduced == outcome::accepted) result.accept_strict();
        else result.reject_strict();
    }
}

template<query requested>
state check_component(const matrix_integer& matrix, const matrix_scan_result& scan, size_t reduction_depth, size_t maximum_depth)
{
    state result = pre_check::detail::ordinary_checks_scanned<requested>(matrix, scan);
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
state preprocess(const matrix_integer& matrix, size_t reduction_depth, size_t maximum_depth)
{
    progress::preprocessing_stage(progress::preprocessing_phase::matrix_scan, matrix.rows(), 0, matrix.rows());
    const matrix_scan_requirements requirements = pre_check::detail::preprocessing_requirements<requested>();
    const matrix_scan_result root_scan = scan_matrix(matrix, requirements);
    state result = pre_check::detail::root_checks_scanned<requested>(matrix, root_scan);
    if (result.done<requested>()) return result;

    state components;
    components.accept_strict();
    std::vector<size_t> indices;
    indices.reserve(matrix.rows());
    size_t component_number = 0;

    progress::preprocessing_stage(progress::preprocessing_phase::connected_components, matrix.rows(), 0, matrix.rows());
    connected_components::visit(root_scan.negative_neighbors, [&](const support& component, bool is_whole_graph) {
        ++component_number;
        state current;
        if (is_whole_graph) {
            current = check_component<requested>(matrix, root_scan, reduction_depth, maximum_depth);
        } else {
            component.copy_indices_to(indices);
            progress::preprocessing_stage(progress::preprocessing_phase::component_scan, indices.size(), component_number);
            scanned_principal_matrix part = scan_principal_matrix(matrix, indices, requirements);
            current = check_component<requested>(part.matrix, part.scan, reduction_depth, maximum_depth);
        }
        components.combine_by_and(current);
        return !should_stop<requested>(components);
    });
    result.merge(components);
    return result;
}

} // namespace detail

/* Fixed exact preprocessing followed by the selected model only when preprocessing cannot decide the requested property. */
template<typename FinalAlgorithm>
bool check(const matrix_integer& matrix, model::copositivity_mode mode, const options& selected, FinalAlgorithm&& final_algorithm)
{
    if (!selected.preprocessing_enabled) return final_algorithm(matrix);
    if (mode == model::copositivity_mode::strictly_copositive) {
        const detail::state result = detail::preprocess<detail::query::strict>(matrix, 0, detail::maximum_reduction_depth);
        if (result.strict_known) return result.value.is_strictly_copositive;
    } else {
        const detail::state result = detail::preprocess<detail::query::copositive>(matrix, 0, detail::maximum_reduction_depth);
        if (result.copositive_known) return result.value.is_copositive;
    }
    progress::preprocessing_stage(progress::preprocessing_phase::model_delegation, matrix.rows());
    return final_algorithm(matrix);
}

template<typename FinalClassifier>
model::copositivity_classification classify(const matrix_integer& matrix, const options& selected,
                                            FinalClassifier&& final_classifier)
{
    if (!selected.preprocessing_enabled) return final_classifier(matrix);
    detail::state result = detail::preprocess<detail::query::combined>(matrix, 0, detail::maximum_reduction_depth);
    if (!result.done<detail::query::combined>()) {
        progress::preprocessing_stage(progress::preprocessing_phase::model_delegation, matrix.rows());
        result.merge(final_classifier(matrix));
    }
    return result.value;
}

} // namespace coposit::component_pipeline
