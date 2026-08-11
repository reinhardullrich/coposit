#pragma once

#include <coposit/connected_components.hpp>
#include <coposit/matrix_scan.hpp>
#include <coposit/pre_check.hpp>

#include <cstddef>
#include <vector>

namespace coposit::component_pipeline {

struct options {
    bool pre_checks_enabled = true;
    bool connected_components = true;
    pre_check::options pre_checks;
};

namespace detail {

template<pre_check::detail::query requested>
bool should_stop(model::copositivity_classification result) noexcept
{
    if constexpr (requested == pre_check::detail::query::ordinary) return !result.is_copositive;
    if constexpr (requested == pre_check::detail::query::strict) return !result.is_strictly_copositive;
    return !result.is_copositive;
}

template<pre_check::detail::query requested, typename FinalClassifier>
model::copositivity_classification run(const matrix_integer& matrix, const pre_check::options& selected,
                                       FinalClassifier& final_classifier)
{
    pre_check::detail::validate_options(selected);

    pre_check::options root_options = selected;
    root_options.frank_wolfe = false;
    root_options.positive_definiteness = false;
    const matrix_scan_result root_scan =
        scan_matrix(matrix, pre_check::detail::requirements_for<requested>(selected, true));

    auto finish = [&](const matrix_integer&) {
        pre_check::options component_options = selected;
        component_options.principal_submatrices = false;
        const matrix_scan_requirements requirements = pre_check::detail::requirements_for<requested>(component_options);
        model::copositivity_classification aggregate{true, true};
        std::vector<size_t> indices;
        indices.reserve(matrix.rows());

        connected_components::visit(root_scan.negative_neighbors, [&](const support& component, bool is_whole_graph) {
            if (is_whole_graph) {
                pre_check::options remaining = pre_check::options::none();
                remaining.frank_wolfe = selected.frank_wolfe;
                remaining.positive_definiteness = selected.positive_definiteness;
                aggregate = pre_check::detail::run_scanned<requested>(matrix, remaining, root_scan, final_classifier);
                return false;
            }

            component.copy_indices_to(indices);
            model::copositivity_classification current;
            if (component_options.small_dimension && indices.size() == 1) {
                const int sign = root_scan.diagonal_signs[indices.front()];
                current = {sign >= 0, sign > 0};
            } else {
                scanned_principal_matrix part = scan_principal_matrix(matrix, indices, requirements);
                current = pre_check::detail::run_scanned<requested>(part.matrix, component_options, part.scan, final_classifier);
            }
            aggregate.is_copositive &= current.is_copositive;
            aggregate.is_strictly_copositive &= current.is_strictly_copositive;
            return !should_stop<requested>(aggregate);
        });
        return aggregate;
    };
    return pre_check::detail::run_scanned<requested>(matrix, root_options, root_scan, finish);
}

} // namespace detail

/* Opt-in negative-entry component decomposition followed by the selected pre-checks and final algorithm. */
template<typename FinalAlgorithm>
bool check(const matrix_integer& matrix, model::copositivity_mode mode, const options& selected,
           FinalAlgorithm&& final_algorithm)
{
    const pre_check::options checks = selected.pre_checks_enabled ? selected.pre_checks : pre_check::options::none();
    if (!selected.connected_components) {
        return pre_check::check(matrix, mode, checks, final_algorithm);
    }
    if (mode == model::copositivity_mode::strictly_copositive) {
        auto classifier = [&](const matrix_integer& part) {
            const bool result = final_algorithm(part);
            return model::copositivity_classification{result, result};
        };
        return detail::run<pre_check::detail::query::strict>(matrix, checks, classifier).is_strictly_copositive;
    }

    auto classifier = [&](const matrix_integer& part) {
        const bool result = final_algorithm(part);
        return model::copositivity_classification{result, false};
    };
    return detail::run<pre_check::detail::query::ordinary>(matrix, checks, classifier).is_copositive;
}

template<typename FinalClassifier>
model::copositivity_classification classify(const matrix_integer& matrix, const options& selected,
                                            FinalClassifier&& final_classifier)
{
    const pre_check::options checks = selected.pre_checks_enabled ? selected.pre_checks : pre_check::options::none();
    if (!selected.connected_components) return pre_check::classify(matrix, checks, final_classifier);
    return detail::run<pre_check::detail::query::combined>(matrix, checks, final_classifier);
}

} // namespace coposit::component_pipeline
