#pragma once

#include <coposit/parsers/fracessa_matrix_parser.hpp>
#include <coposit/parsers/matrix_market_parser.hpp>

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace coposit::parsers::matrix_parser {

inline matrix_integer parse(std::string_view input)
{
    if (!input.empty() && input.back() == '\n') {
        input.remove_suffix(1);
        if (!input.empty() && input.back() == '\r') input.remove_suffix(1);
    }
    return matrix_market_parser::has_banner(input) ? matrix_market_parser::parse(input) : fracessa_matrix_parser::parse(input);
}

inline matrix_integer parse_file(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) throw std::invalid_argument("cannot open matrix file: " + filename);
    const std::streamoff file_size = file.tellg();
    std::string input;
    if (file_size < 0 || static_cast<std::uintmax_t>(file_size) > input.max_size()
        || static_cast<std::uintmax_t>(file_size) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::invalid_argument("matrix file is too large: " + filename);
    }

    input.resize(static_cast<size_t>(file_size));
    file.seekg(0);
    file.read(input.data(), static_cast<std::streamsize>(input.size()));
    if (!file) throw std::invalid_argument("cannot read matrix file: " + filename);
    return parse(input);
}

} // namespace coposit::parsers::matrix_parser
