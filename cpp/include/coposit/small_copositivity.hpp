#pragma once

#include <coposit/model.hpp>

#include <cassert>
#include <cstddef>

namespace coposit::small_copositivity {

template<model::copositivity_mode mode>
inline bool positive_for_mode(int sign) noexcept
{
    if constexpr (mode == model::copositivity_mode::strictly_copositive) return sign > 0;
    return sign >= 0;
}

template<model::copositivity_mode mode = model::copositivity_mode::strictly_copositive>
inline bool check_1x1(integer::const_reference b11) noexcept
{
    return positive_for_mode<mode>(b11.sign());
}

template<model::copositivity_mode mode = model::copositivity_mode::strictly_copositive>
inline bool check_2x2(integer::const_reference b11, integer::const_reference b12, integer::const_reference b22) noexcept
{
    if (!positive_for_mode<mode>(b11.sign()) || !positive_for_mode<mode>(b22.sign())) return false;
    if (b12.sign() >= 0) return true;

    integer determinant;
    determinant.set_product(b11, b22);
    determinant.submul(b12, b12);
    return positive_for_mode<mode>(determinant.sign());
}

template<model::copositivity_mode mode = model::copositivity_mode::strictly_copositive>
inline bool check_3x3(integer::const_reference b11, integer::const_reference b12, integer::const_reference b13,
                      integer::const_reference b22, integer::const_reference b23, integer::const_reference b33) noexcept
{
    if (!positive_for_mode<mode>(b11.sign()) || !positive_for_mode<mode>(b22.sign())
        || !positive_for_mode<mode>(b33.sign())) return false;

    integer work;
    if (b12.sign() < 0) {
        work.set_product(b11, b22);
        work.submul(b12, b12);
        if (!positive_for_mode<mode>(work.sign())) return false;
    }
    if (b13.sign() < 0) {
        work.set_product(b11, b33);
        work.submul(b13, b13);
        if (!positive_for_mode<mode>(work.sign())) return false;
    }
    if (b23.sign() < 0) {
        work.set_product(b22, b33);
        work.submul(b23, b23);
        if (!positive_for_mode<mode>(work.sign())) return false;
    }

    integer determinant;
    determinant.set_product(b11, b22);
    fmpz_mul(determinant.native_handle(), determinant.native_handle(), b33.native_handle());
    work.set_product(b12, b13);
    fmpz_mul(work.native_handle(), work.native_handle(), b23.native_handle());
    work.multiply(2);
    determinant += work;
    work.set_product(b23, b23);
    determinant.submul(b11, work);
    work.set_product(b13, b13);
    determinant.submul(b22, work);
    work.set_product(b12, b12);
    determinant.submul(b33, work);

    if (positive_for_mode<mode>(determinant.sign())) return true;

    work.set_product(b22, b33);
    work.submul(b23, b23);
    if (!positive_for_mode<mode>(work.sign())) return true;
    work.set_product(b11, b33);
    work.submul(b13, b13);
    if (!positive_for_mode<mode>(work.sign())) return true;
    work.set_product(b11, b22);
    work.submul(b12, b12);
    if (!positive_for_mode<mode>(work.sign())) return true;
    work.set_product(b13, b23);
    work.submul(b12, b33);
    if (!positive_for_mode<mode>(work.sign())) return true;
    work.set_product(b12, b23);
    work.submul(b13, b22);
    if (!positive_for_mode<mode>(work.sign())) return true;
    work.set_product(b12, b13);
    work.submul(b11, b23);
    return !positive_for_mode<mode>(work.sign());
}

template<model::copositivity_mode mode = model::copositivity_mode::strictly_copositive>
inline bool check_principal(const matrix_integer& matrix, const size_t* indices, size_t dimension) noexcept
{
    assert(indices != nullptr);
    assert(dimension >= 1 && dimension <= 3);

    switch (dimension) {
        case 1:
            return check_1x1<mode>(matrix(indices[0], indices[0]));
        case 2:
            return check_2x2<mode>(matrix(indices[0], indices[0]), matrix(indices[0], indices[1]), matrix(indices[1], indices[1]));
        case 3:
            return check_3x3<mode>(
                matrix(indices[0], indices[0]), matrix(indices[0], indices[1]), matrix(indices[0], indices[2]),
                matrix(indices[1], indices[1]), matrix(indices[1], indices[2]), matrix(indices[2], indices[2]));
    }
    return false;
}

template<model::copositivity_mode mode = model::copositivity_mode::strictly_copositive>
inline bool check(const matrix_integer& matrix) noexcept
{
    assert(matrix.rows() == matrix.cols());
    assert(matrix.rows() >= 1 && matrix.rows() <= 3);
    constexpr size_t indices[] = {0, 1, 2};
    return check_principal<mode>(matrix, indices, matrix.rows());
}

inline bool check_principal(const matrix_integer& matrix, const size_t* indices, size_t dimension,
                            model::copositivity_mode mode) noexcept
{
    if (mode == model::copositivity_mode::strictly_copositive) {
        return check_principal<model::copositivity_mode::strictly_copositive>(matrix, indices, dimension);
    }
    return check_principal<model::copositivity_mode::copositive>(matrix, indices, dimension);
}

inline bool check(const matrix_integer& matrix, model::copositivity_mode mode) noexcept
{
    if (mode == model::copositivity_mode::strictly_copositive) {
        return check<model::copositivity_mode::strictly_copositive>(matrix);
    }
    return check<model::copositivity_mode::copositive>(matrix);
}

inline model::copositivity_classification classify_principal(const matrix_integer& matrix, const size_t* indices,
                                                              size_t dimension) noexcept
{
    return {
        check_principal<model::copositivity_mode::copositive>(matrix, indices, dimension),
        check_principal<model::copositivity_mode::strictly_copositive>(matrix, indices, dimension),
    };
}

inline model::copositivity_classification classify(const matrix_integer& matrix) noexcept
{
    assert(matrix.rows() == matrix.cols());
    assert(matrix.rows() >= 1 && matrix.rows() <= 3);
    constexpr size_t indices[] = {0, 1, 2};
    return classify_principal(matrix, indices, matrix.rows());
}

} // namespace coposit::small_copositivity
