#pragma once

#include <flint/fmpz_mat.h>

#include <cstddef>
#include <sstream>
#include <string>

#include <coposit/integer.hpp>

namespace coposit {

/*
 * Owning C++ wrapper around FLINT's row-major arbitrary-precision integer matrix.
 *
 * Entry access returns a non-owning integer reference, so reads and in-place writes use the original FLINT storage without a copy
 * or allocation. The native handle provides direct FLINT interoperability for exact numerical kernels.
 */
class matrix_integer {
public:
    matrix_integer() noexcept { fmpz_mat_init(data_, 0, 0); }

    matrix_integer(size_t rows, size_t columns)
    {
        fmpz_mat_init(data_, static_cast<slong>(rows), static_cast<slong>(columns));
    }

    matrix_integer(const matrix_integer& other) { fmpz_mat_init_set(data_, other.data_); }

    matrix_integer(matrix_integer&& other) noexcept
    {
        fmpz_mat_init(data_, 0, 0);
        fmpz_mat_swap(data_, other.data_);
    }

    ~matrix_integer() noexcept { fmpz_mat_clear(data_); }

    matrix_integer& operator=(const matrix_integer& other)
    {
        if (this != &other) {
            matrix_integer copy(other);
            swap(copy);
        }
        return *this;
    }

    matrix_integer& operator=(matrix_integer&& other) noexcept
    {
        if (this != &other) fmpz_mat_swap(data_, other.data_);
        return *this;
    }

    size_t rows() const noexcept { return static_cast<size_t>(fmpz_mat_nrows(data_)); }
    size_t cols() const noexcept { return static_cast<size_t>(fmpz_mat_ncols(data_)); }

    integer::reference operator()(size_t row, size_t column) noexcept
    {
        return integer::reference(fmpz_mat_entry(data_, static_cast<slong>(row), static_cast<slong>(column)));
    }

    integer::const_reference operator()(size_t row, size_t column) const noexcept
    {
        return integer::const_reference(fmpz_mat_entry(data_, static_cast<slong>(row), static_cast<slong>(column)));
    }

    void resize(size_t rows, size_t columns)
    {
        if (this->rows() == rows && this->cols() == columns) return;
        matrix_integer replacement(rows, columns);
        swap(replacement);
    }

    void set_identity(size_t dimension)
    {
        resize(dimension, dimension);
        fmpz_mat_one(data_);
    }

    void negate() noexcept { fmpz_mat_neg(data_, data_); }

    // Human-readable row-labelled formatting for diagnostics in callers and embedders such as FracESSA.
    std::string to_pretty_string() const
    {
        std::stringstream stream;
        for (size_t row = 0; row < rows(); ++row) {
            stream << "  " << row << ": [";
            for (size_t column = 0; column < cols(); ++column) {
                if (column != 0) stream << ", ";
                stream << integer((*this)(row, column)).to_string();
            }
            stream << ']';
            if (row + 1 != rows()) stream << '\n';
        }
        return stream.str();
    }

    void swap(matrix_integer& other) noexcept { fmpz_mat_swap(data_, other.data_); }

    fmpz_mat_struct* native_handle() noexcept { return data_; }
    const fmpz_mat_struct* native_handle() const noexcept { return data_; }

private:
    fmpz_mat_t data_;
};

inline void swap(matrix_integer& left, matrix_integer& right) noexcept
{
    left.swap(right);
}

} // namespace coposit
