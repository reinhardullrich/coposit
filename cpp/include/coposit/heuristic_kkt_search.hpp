#pragma once

#include <coposit/matrix_integer.hpp>

#include <cstddef>
#include <optional>

namespace coposit::pre_check::detail {

struct heuristic_kkt_result {
    std::optional<int> sign;
    size_t visited = 0;
    bool exact_continuation = false;
    bool reached_kkt = false;
};

heuristic_kkt_result run_heuristic_kkt_search(const matrix_integer& matrix);

} // namespace coposit::pre_check::detail
