#pragma once

#include <coposit/matrix_integer.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace coposit {

namespace detail {

inline std::string_view trim(std::string_view value) noexcept
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

} // namespace detail

/* Parse dimension#upper-triangle-values, with exact base-10 integers in row-major upper-triangle order. */
inline matrix_integer parse_integer_matrix(std::string_view input)
{
    input = detail::trim(input);
    const size_t separator = input.find('#');
    if (separator == std::string_view::npos) throw std::invalid_argument("expected dimension#upper-triangle-values");

    const std::string_view dimension_text = detail::trim(input.substr(0, separator));
    size_t dimension = 0;
    const auto parsed_dimension = std::from_chars(
        dimension_text.data(), dimension_text.data() + dimension_text.size(), dimension);
    if (dimension_text.empty() || parsed_dimension.ec != std::errc{} || parsed_dimension.ptr != dimension_text.data() + dimension_text.size()
        || dimension == 0 || dimension > static_cast<size_t>(std::numeric_limits<slong>::max())) {
        throw std::invalid_argument("dimension must be a positive integer supported by FLINT");
    }
    if (dimension > std::numeric_limits<size_t>::max() / (dimension + 1)) {
        throw std::invalid_argument("matrix dimension is too large");
    }

    const size_t expected_values = dimension * (dimension + 1) / 2;
    const std::string_view values = detail::trim(input.substr(separator + 1));
    const size_t supplied_values = values.empty() ? 0 : 1 + static_cast<size_t>(std::count(values.begin(), values.end(), ','));
    if (supplied_values != expected_values) throw std::invalid_argument("wrong number of upper-triangle values");

    matrix_integer matrix(dimension, dimension);
    size_t offset = 0;
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row; column < dimension; ++column) {
            const size_t comma = values.find(',', offset);
            const std::string_view token = detail::trim(values.substr(offset, comma - offset));
            const std::string terminated(token);
            if (token.empty() || fmpz_set_str(matrix(row, column).native_handle(), terminated.c_str(), 10) != 0) {
                throw std::invalid_argument("matrix values must be base-10 integers");
            }
            matrix(column, row) = matrix(row, column);
            offset = comma == std::string_view::npos ? values.size() : comma + 1;
        }
    }

    return matrix;
}

} // namespace coposit
