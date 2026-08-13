#pragma once

#include <coposit/parsers/exact_number_parser.hpp>
#include <coposit/parsers/parsed_matrix.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace coposit::parsers::fracessa_matrix_parser {

/* Parse FracESSA's dimension#values format: a full upper triangle or its short circular-symmetric form. */
inline parsed_matrix parse(std::string_view input)
{
    if (input.find_first_of(" \t\r\n") != std::string_view::npos) {
        throw std::invalid_argument("compact FracESSA matrix must not contain whitespace");
    }
    const size_t separator = input.find('#');
    if (separator == std::string_view::npos) throw std::invalid_argument("expected dimension#upper-triangle-values");

    const std::string_view dimension_text = input.substr(0, separator);
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
    const size_t expected_circular_values = dimension / 2 + 1;
    const std::string_view values = input.substr(separator + 1);
    const size_t supplied_values = values.empty() ? 0 : 1 + static_cast<size_t>(std::count(values.begin(), values.end(), ','));
    const bool circular_symmetric = dimension >= 2 && supplied_values == expected_circular_values;
    if (supplied_values != expected_values && !circular_symmetric) {
        throw std::invalid_argument("wrong number of FracESSA matrix values");
    }

    const bool integer_values = values.find_first_of("./eE") == std::string_view::npos;
    integer common_denominator(1);
    size_t offset = 0;
    if (!integer_values) {
        for (size_t index = 0; index < supplied_values; ++index) {
            const size_t comma = values.find(',', offset);
            const std::string_view token = values.substr(offset, comma - offset);
            try {
                const exact_number_parser::exact_rational parsed = exact_number_parser::parse(token, true);
                if (!parsed.denominator.is_one()) {
                    fmpz_lcm(common_denominator.native_handle(), common_denominator.native_handle(), parsed.denominator.native_handle());
                }
            } catch (const std::invalid_argument&) {
                throw std::invalid_argument("matrix values must be exact integers, fractions, decimals, or scientific numbers");
            }
            offset = comma == std::string_view::npos ? values.size() : comma + 1;
        }
    }

    const auto set_value = [&](integer::reference destination, std::string_view token) {
        try {
            if (integer_values) exact_number_parser::set_integer(destination, token);
            else exact_number_parser::set_scaled(destination, exact_number_parser::parse(token, true), common_denominator);
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("matrix values must be exact integers, fractions, decimals, or scientific numbers");
        }
    };

    matrix_integer matrix(dimension, dimension);
    offset = 0;
    if (circular_symmetric) {
        std::vector<integer> distances(supplied_values);
        for (integer& value : distances) {
            const size_t comma = values.find(',', offset);
            set_value(value.ref(), values.substr(offset, comma - offset));
            offset = comma == std::string_view::npos ? values.size() : comma + 1;
        }
        for (size_t row = 0; row < dimension; ++row) {
            matrix(row, row) = distances[0];
            for (size_t column = row + 1; column < dimension; ++column) {
                const size_t direct_distance = column - row;
                const size_t circular_distance = std::min(direct_distance, dimension - direct_distance);
                matrix(row, column) = distances[circular_distance];
                matrix(column, row) = matrix(row, column);
            }
        }
        return {std::move(matrix), std::move(common_denominator), true};
    }

    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row; column < dimension; ++column) {
            const size_t comma = values.find(',', offset);
            const std::string_view token = values.substr(offset, comma - offset);
            set_value(matrix(row, column), token);
            matrix(column, row) = matrix(row, column);
            offset = comma == std::string_view::npos ? values.size() : comma + 1;
        }
    }

    return {std::move(matrix), std::move(common_denominator), false};
}

} // namespace coposit::parsers::fracessa_matrix_parser
