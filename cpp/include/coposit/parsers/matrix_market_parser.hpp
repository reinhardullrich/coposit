#pragma once

#include <coposit/parsers/exact_number_parser.hpp>
#include <coposit/parsers/parsed_matrix.hpp>

#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace coposit::parsers::matrix_market_parser {

namespace detail {

inline std::string_view trim(std::string_view value) noexcept
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

enum class storage { coordinate, array };
enum class field { real, complex, integer, pattern };

inline bool ascii_equal(std::string_view left, std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (size_t index = 0; index < left.size(); ++index) {
        char a = left[index];
        char b = right[index];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

class fields {
public:
    explicit fields(std::string_view line)
        : remaining_(line)
    {
    }

    std::string_view next() noexcept
    {
        const size_t first = remaining_.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            remaining_ = {};
            return {};
        }
        remaining_.remove_prefix(first);
        const size_t end = remaining_.find_first_of(" \t\r\n");
        const std::string_view result = remaining_.substr(0, end);
        remaining_ = end == std::string_view::npos ? std::string_view{} : remaining_.substr(end);
        return result;
    }

private:
    std::string_view remaining_;
};

struct line_record {
    size_t number = 0;
    std::string_view text;
};

class lines {
public:
    explicit lines(std::string_view input, size_t preceding_lines = 0)
        : input_(input)
        , line_number_(preceding_lines)
    {
    }

    bool next_raw(line_record& result) noexcept
    {
        if (position_ >= input_.size()) return false;
        const size_t newline = input_.find('\n', position_);
        const size_t end = newline == std::string_view::npos ? input_.size() : newline;
        result = {++line_number_, input_.substr(position_, end - position_)};
        if (!result.text.empty() && result.text.back() == '\r') result.text.remove_suffix(1);
        position_ = newline == std::string_view::npos ? input_.size() : newline + 1;
        return true;
    }

    bool next_data(line_record& result) noexcept
    {
        while (next_raw(result)) {
            result.text = trim(result.text);
            if (!result.text.empty() && result.text.front() != '%') return true;
        }
        return false;
    }

    size_t position() const noexcept { return position_; }
    size_t line_number() const noexcept { return line_number_; }

private:
    std::string_view input_;
    size_t position_ = 0;
    size_t line_number_ = 0;
};

[[noreturn]] inline void line_error(size_t line, const std::string& message)
{
    throw std::invalid_argument("Matrix Market line " + std::to_string(line) + ": " + message);
}

inline size_t parse_size(std::string_view token, size_t line, const char* name)
{
    size_t value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (token.empty() || parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) {
        line_error(line, std::string(name) + " must be a nonnegative integer");
    }
    return value;
}

inline void require_end(fields& input, size_t line)
{
    if (!input.next().empty()) line_error(line, "too many fields");
}

struct document {
    storage stored_as;
    field value_field;
    size_t rows = 0;
    size_t columns = 0;
    size_t entries = 0;
    std::string_view data;
    size_t preceding_data_lines = 0;
};

inline document parse_document_header(std::string_view input)
{
    lines reader(input);
    line_record line;
    if (!reader.next_raw(line)) throw std::invalid_argument("empty Matrix Market input");
    fields banner(trim(line.text));
    if (!ascii_equal(banner.next(), "%%MatrixMarket") || !ascii_equal(banner.next(), "matrix")) {
        line_error(line.number, "expected '%%MatrixMarket matrix' banner");
    }

    document result;
    const std::string_view storage_name = banner.next();
    if (ascii_equal(storage_name, "coordinate")) result.stored_as = storage::coordinate;
    else if (ascii_equal(storage_name, "array")) result.stored_as = storage::array;
    else line_error(line.number, "storage must be 'coordinate' or 'array'");

    const std::string_view field_name = banner.next();
    if (ascii_equal(field_name, "real")) result.value_field = field::real;
    else if (ascii_equal(field_name, "complex")) result.value_field = field::complex;
    else if (ascii_equal(field_name, "integer")) result.value_field = field::integer;
    else if (ascii_equal(field_name, "pattern")) result.value_field = field::pattern;
    else line_error(line.number, "field must be 'real', 'complex', 'integer', or 'pattern'");

    const std::string_view symmetry_name = banner.next();
    if (!ascii_equal(symmetry_name, "symmetric")) {
        line_error(line.number, "Coposit accepts only Matrix Market matrices declared 'symmetric'");
    }
    require_end(banner, line.number);

    if (result.stored_as == storage::array && result.value_field == field::pattern) {
        line_error(line.number, "pattern field is valid only with coordinate storage");
    }
    if (!reader.next_data(line)) throw std::invalid_argument("Matrix Market input has no size line");
    fields size_fields(line.text);
    result.rows = parse_size(size_fields.next(), line.number, "row count");
    result.columns = parse_size(size_fields.next(), line.number, "column count");
    if (result.stored_as == storage::coordinate) {
        result.entries = parse_size(size_fields.next(), line.number, "entry count");
    }
    require_end(size_fields, line.number);

    if (result.rows > static_cast<size_t>(std::numeric_limits<slong>::max())
        || result.columns > static_cast<size_t>(std::numeric_limits<slong>::max())
        || (result.rows != 0 && result.columns > std::numeric_limits<size_t>::max() / result.rows)) {
        line_error(line.number, "matrix dimensions are too large");
    }
    if (result.rows == 0 || result.rows != result.columns) line_error(line.number, "symmetric matrices must be nonempty and square");

    result.data = input.substr(reader.position());
    result.preceding_data_lines = reader.line_number();
    return result;
}

template <typename Visitor>
inline void visit_entries(const document& input, Visitor&& visitor)
{
    lines reader(input.data, input.preceding_data_lines);
    line_record line;

    const auto read_value = [&](fields& entry, size_t row, size_t column, size_t line_number) {
        std::string_view real_part;
        std::string_view imaginary_part;
        if (input.value_field != field::pattern) {
            real_part = entry.next();
            if (real_part.empty()) line_error(line_number, "matrix entry has no value");
            if (input.value_field == field::complex) {
                imaginary_part = entry.next();
                if (imaginary_part.empty()) line_error(line_number, "complex matrix entry has no imaginary part");
            }
        }
        require_end(entry, line_number);
        visitor(row, column, real_part, imaginary_part, line_number);
    };

    if (input.stored_as == storage::coordinate) {
        for (size_t index = 0; index < input.entries; ++index) {
            if (!reader.next_data(line)) line_error(input.preceding_data_lines, "fewer coordinate entries than declared");
            fields entry(line.text);
            const size_t one_based_row = parse_size(entry.next(), line.number, "row index");
            const size_t one_based_column = parse_size(entry.next(), line.number, "column index");
            if (one_based_row == 0 || one_based_row > input.rows || one_based_column == 0 || one_based_column > input.columns) {
                line_error(line.number, "coordinate is outside the matrix");
            }
            const size_t row = one_based_row - 1;
            const size_t column = one_based_column - 1;
            if (row < column) line_error(line.number, "symmetric coordinate must be in the lower triangle");
            read_value(entry, row, column, line.number);
        }
    } else {
        const auto consume = [&](size_t row, size_t column) {
            if (!reader.next_data(line)) line_error(input.preceding_data_lines, "too few array entries");
            fields entry(line.text);
            read_value(entry, row, column, line.number);
        };
        for (size_t column = 0; column < input.columns; ++column) {
            for (size_t row = column; row < input.rows; ++row) consume(row, column);
        }
    }

    if (reader.next_data(line)) line_error(line.number, "more matrix entries than declared");
}

inline exact_number_parser::exact_rational parse_value(field value_field, std::string_view token, size_t line)
{
    if (value_field == field::pattern) {
        exact_number_parser::exact_rational one;
        one.numerator.set_one();
        return one;
    }
    try {
        return exact_number_parser::parse(token, false, value_field == field::integer);
    } catch (const std::invalid_argument& error) {
        line_error(line, error.what());
    }
}

} // namespace detail

/* Parse a NIST Matrix Market matrix declared symmetric and convert its exact real values to one integer scale. */
inline parsed_matrix parse(std::string_view input)
{
    input = detail::trim(input);
    const detail::document document = detail::parse_document_header(input);

    const auto fill_matrix = [&](auto&& set_value) {
        matrix_integer matrix(document.rows, document.columns);
        std::vector<bool> seen;
        if (document.stored_as == detail::storage::coordinate) seen.resize(document.rows * document.columns);
        detail::visit_entries(document, [&](size_t row, size_t column, std::string_view real_token, std::string_view, size_t line) {
            if (!seen.empty()) {
                const size_t position = row * document.columns + column;
                if (seen[position]) detail::line_error(line, "duplicate coordinate");
                seen[position] = true;
            }

            set_value(matrix(row, column), real_token, line);
            if (row != column) matrix(column, row) = matrix(row, column);
        });
        return matrix;
    };

    if (document.value_field == detail::field::integer || document.value_field == detail::field::pattern) {
        matrix_integer matrix = fill_matrix([&](integer::reference destination, std::string_view token, size_t line) {
            if (document.value_field == detail::field::pattern) {
                destination.set_one();
                return;
            }
            try {
                exact_number_parser::set_integer(destination, token);
            } catch (const std::invalid_argument& error) {
                detail::line_error(line, error.what());
            }
        });
        return {std::move(matrix), integer(1), false};
    }

    integer common_denominator(1);
    bool has_nonzero_imaginary_part = false;
    detail::visit_entries(document, [&](size_t, size_t, std::string_view real_token, std::string_view imaginary_token, size_t line) {
        const exact_number_parser::exact_rational real_value = detail::parse_value(document.value_field, real_token, line);
        if (!real_value.denominator.is_one()) {
            fmpz_lcm(common_denominator.native_handle(), common_denominator.native_handle(), real_value.denominator.native_handle());
        }
        if (document.value_field == detail::field::complex) {
            const exact_number_parser::exact_rational imaginary_value = detail::parse_value(detail::field::real, imaginary_token, line);
            has_nonzero_imaginary_part = has_nonzero_imaginary_part || !imaginary_value.numerator.is_zero();
        }
    });
    if (has_nonzero_imaginary_part) {
        throw std::invalid_argument("Coposit requires a real matrix; a complex Matrix Market entry has a nonzero imaginary part");
    }

    matrix_integer matrix = fill_matrix([&](integer::reference destination, std::string_view real_token, size_t line) {
        const exact_number_parser::exact_rational value = detail::parse_value(document.value_field, real_token, line);
        exact_number_parser::set_scaled(destination, value, common_denominator);
    });
    return {std::move(matrix), std::move(common_denominator), false};
}

inline bool has_banner(std::string_view input) noexcept
{
    const std::string_view trimmed = detail::trim(input);
    const size_t first_end = trimmed.find_first_of(" \t\r\n");
    const std::string_view first = trimmed.substr(0, first_end);
    return detail::ascii_equal(first, "%%MatrixMarket");
}

} // namespace coposit::parsers::matrix_market_parser
