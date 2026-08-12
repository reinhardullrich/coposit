#pragma once

#include <coposit/matrix_integer.hpp>

namespace coposit::parsers {

/* Exact parsed input A = matrix / denominator, where denominator is positive. */
struct parsed_matrix {
    matrix_integer matrix;
    integer denominator{1};
    bool compact_circular = false;
};

} // namespace coposit::parsers
