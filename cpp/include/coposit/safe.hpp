#pragma once

#include <coposit/component_pipeline.hpp>

namespace coposit::safe {

/* Exact strict-copositivity decision for a nonempty square symmetric integer matrix. */
inline bool is_strictly_copositive(const matrix_integer& matrix)
{
    return component_pipeline::check(
        matrix, model::copositivity_mode::strictly_copositive, component_pipeline::options{},
        [](const matrix_integer& component) {
            return model::solve(component, model::copositivity_mode::strictly_copositive);
        });
}

} // namespace coposit::safe
