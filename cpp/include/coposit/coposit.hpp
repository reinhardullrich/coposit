#pragma once

#include <coposit/incumbent.hpp>
#include <coposit/matrix_integer.hpp>

#include <optional>

namespace coposit {

enum class copositivity_mode { copositive, strictly_copositive, both };

struct copositivity_result {
    std::optional<bool> is_copositive;
    std::optional<bool> is_strictly_copositive;
};

/* Exact public interface using the current incumbent and the complete preprocessing pipeline. */
copositivity_result check(const matrix_integer& matrix, copositivity_mode mode = copositivity_mode::both);

} // namespace coposit
