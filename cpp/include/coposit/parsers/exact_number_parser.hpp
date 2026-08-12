#pragma once

#include <coposit/integer.hpp>

#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace coposit::parsers::exact_number_parser {

struct exact_decimal {
    integer significand;
    slong exponent = 0; // value = significand * 10^exponent
};

struct exact_rational {
    integer numerator;
    integer denominator{1};
};

inline void set_integer(integer::reference destination, std::string_view token)
{
    if (!token.empty() && token.front() == '+') {
        token.remove_prefix(1);
        if (token.empty() || token.front() == '+' || token.front() == '-') throw std::invalid_argument("invalid decimal number");
    }
    slong small = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), small);
    if (token.empty() || parsed.ptr != token.data() + token.size()
        || (parsed.ec != std::errc{} && parsed.ec != std::errc::result_out_of_range)) {
        throw std::invalid_argument("invalid decimal number");
    }
    if (parsed.ec == std::errc{}) {
        fmpz_set_si(destination.native_handle(), small);
        return;
    }

    const std::string copy(token);
    if (fmpz_set_str(destination.native_handle(), copy.c_str(), 10) != 0) {
        throw std::invalid_argument("invalid decimal number");
    }
}

inline slong parse_decimal_exponent(std::string_view text)
{
    if (text.empty()) throw std::invalid_argument("decimal exponent has no digits");
    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) throw std::invalid_argument("decimal exponent has no digits");

    const ulong limit = static_cast<ulong>(std::numeric_limits<slong>::max());
    ulong magnitude = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') throw std::invalid_argument("invalid decimal exponent");
        const ulong digit = static_cast<ulong>(character - '0');
        if (magnitude > (limit - digit) / 10) throw std::invalid_argument("decimal exponent is too large");
        magnitude = magnitude * 10 + digit;
    }
    const slong value = static_cast<slong>(magnitude);
    return negative ? -value : value;
}

inline exact_decimal parse_exact_decimal(std::string_view token, bool integer_only = false)
{
    if (token.empty()) throw std::invalid_argument("number is empty");

    const size_t exponent_marker = token.find_first_of("eE");
    if (integer_only && (exponent_marker != std::string_view::npos || token.find('.') != std::string_view::npos)) {
        throw std::invalid_argument("integer field requires a base-10 integer");
    }
    if (exponent_marker != std::string_view::npos && token.find_first_of("eE", exponent_marker + 1) != std::string_view::npos) {
        throw std::invalid_argument("number has more than one exponent marker");
    }

    const std::string_view mantissa = token.substr(0, exponent_marker);
    slong exponent = exponent_marker == std::string_view::npos ? 0 : parse_decimal_exponent(token.substr(exponent_marker + 1));
    if (mantissa.empty()) throw std::invalid_argument("number has no mantissa");

    bool negative = false;
    size_t position = 0;
    if (mantissa.front() == '+' || mantissa.front() == '-') {
        negative = mantissa.front() == '-';
        position = 1;
    }

    bool decimal_point_seen = false;
    size_t fractional_digits = 0;
    std::string digits;
    digits.reserve(mantissa.size());
    for (; position < mantissa.size(); ++position) {
        const char character = mantissa[position];
        if (character == '.') {
            if (decimal_point_seen) throw std::invalid_argument("number has more than one decimal point");
            decimal_point_seen = true;
            continue;
        }
        if (character < '0' || character > '9') throw std::invalid_argument("invalid decimal number");
        digits.push_back(character);
        if (decimal_point_seen) ++fractional_digits;
    }
    if (digits.empty()) throw std::invalid_argument("number has no digits");
    if (fractional_digits > static_cast<size_t>(std::numeric_limits<slong>::max())
        || exponent < std::numeric_limits<slong>::min() + static_cast<slong>(fractional_digits)) {
        throw std::invalid_argument("decimal exponent is too small");
    }
    exponent -= static_cast<slong>(fractional_digits);

    const size_t first_nonzero = digits.find_first_not_of('0');
    if (first_nonzero == std::string::npos) return {};
    digits.erase(0, first_nonzero);
    while (digits.back() == '0') {
        digits.pop_back();
        if (exponent == std::numeric_limits<slong>::max()) throw std::invalid_argument("decimal exponent is too large");
        ++exponent;
    }
    if (negative) digits.insert(digits.begin(), '-');

    exact_decimal parsed;
    if (fmpz_set_str(parsed.significand.native_handle(), digits.c_str(), 10) != 0) {
        throw std::invalid_argument("invalid decimal number");
    }
    parsed.exponent = exponent;
    return parsed;
}

inline ulong exponent_distance(slong exponent, slong common_exponent)
{
    if (exponent < common_exponent) throw std::logic_error("decimal scale is inconsistent");
    if (common_exponent < 0 && exponent >= 0) {
        return static_cast<ulong>(exponent) + static_cast<ulong>(-(common_exponent + 1)) + 1;
    }
    return static_cast<ulong>(exponent - common_exponent);
}

inline void set_scaled_decimal(integer::reference destination, const exact_decimal& value, slong common_exponent)
{
    if (value.significand.is_zero()) {
        destination.set_zero();
        return;
    }
    fmpz_set(destination.native_handle(), value.significand.native_handle());
    const ulong distance = exponent_distance(value.exponent, common_exponent);
    if (distance == 0) return;

    fmpz_t power;
    fmpz_init_set_ui(power, 10);
    fmpz_pow_ui(power, power, distance);
    fmpz_mul(destination.native_handle(), destination.native_handle(), power);
    fmpz_clear(power);
}

inline void set_power_of_ten(integer& destination, ulong exponent)
{
    if (exponent == 0) {
        destination.set_one();
        return;
    }
    fmpz_set_ui(destination.native_handle(), 10);
    fmpz_pow_ui(destination.native_handle(), destination.native_handle(), exponent);
}

inline void canonicalize_rational(exact_rational& value)
{
    if (value.denominator.is_zero()) throw std::invalid_argument("rational denominator cannot be zero");
    if (value.numerator.is_zero()) {
        value.denominator.set_one();
        return;
    }
    if (value.denominator.sign() < 0) {
        value.numerator.negate();
        value.denominator.negate();
    }
    if (value.denominator.is_one()) return;

    integer divisor;
    fmpz_gcd(divisor.native_handle(), value.numerator.native_handle(), value.denominator.native_handle());
    value.numerator.divide_exact(divisor);
    value.denominator.divide_exact(divisor);
}

inline exact_rational rational_from_decimal(const exact_decimal& decimal)
{
    exact_rational value;
    value.numerator = decimal.significand;
    if (value.numerator.is_zero()) return value;

    integer power;
    if (decimal.exponent >= 0) {
        set_power_of_ten(power, static_cast<ulong>(decimal.exponent));
        fmpz_mul(value.numerator.native_handle(), value.numerator.native_handle(), power.native_handle());
    } else {
        set_power_of_ten(value.denominator, exponent_distance(0, decimal.exponent));
    }
    canonicalize_rational(value);
    return value;
}

/* Both formats use this exact parser; only the compact FracESSA format enables integer fractions. */
inline exact_rational parse(std::string_view token, bool allow_fraction, bool integer_only = false)
{
    const size_t slash = token.find('/');
    if (slash == std::string_view::npos) return rational_from_decimal(parse_exact_decimal(token, integer_only));
    if (!allow_fraction) throw std::invalid_argument("fractions are not part of Matrix Market numeric syntax");
    if (integer_only) throw std::invalid_argument("integer field requires a base-10 integer");
    if (token.find('/', slash + 1) != std::string_view::npos) throw std::invalid_argument("number has more than one fraction bar");

    const std::string_view denominator_token = token.substr(slash + 1);
    if (denominator_token.empty() || denominator_token.front() == '+' || denominator_token.front() == '-') {
        throw std::invalid_argument("a fraction sign is allowed only before its numerator");
    }

    exact_rational value;
    const exact_decimal numerator = parse_exact_decimal(token.substr(0, slash), true);
    const exact_decimal denominator = parse_exact_decimal(denominator_token, true);
    set_scaled_decimal(value.numerator.ref(), numerator, 0);
    set_scaled_decimal(value.denominator.ref(), denominator, 0);
    canonicalize_rational(value);
    return value;
}

inline void set_scaled(integer::reference destination, const exact_rational& value, integer::const_reference common_denominator)
{
    if (value.numerator.is_zero()) {
        destination.set_zero();
        return;
    }
    if (value.denominator.compare(common_denominator) == 0) {
        destination = value.numerator;
        return;
    }
    integer multiplier(common_denominator);
    multiplier.divide_exact(value.denominator);
    destination.set_product(value.numerator, multiplier);
}

} // namespace coposit::parsers::exact_number_parser
