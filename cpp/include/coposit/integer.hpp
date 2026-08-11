#pragma once

#include <flint/fmpz.h>

#include <utility>

namespace coposit {

namespace detail {

// Non-owning views let matrix entries use the same readable operations as an owning integer without copying or allocating.
class integer_const_reference {
public:
    explicit integer_const_reference(const fmpz* value) noexcept
        : value_(value)
    {
    }

    const fmpz* native_handle() const noexcept { return value_; }
    int sign() const noexcept { return fmpz_sgn(value_); }
    bool is_zero() const noexcept { return fmpz_is_zero(value_); }
    bool is_one() const noexcept { return fmpz_is_one(value_); }
    int compare(integer_const_reference other) const noexcept { return fmpz_cmp(value_, other.value_); }
    int compare_abs(integer_const_reference other) const noexcept { return fmpz_cmpabs(value_, other.value_); }
    double to_dbl_2exp(slong& exponent) const noexcept { return fmpz_get_d_2exp(&exponent, value_); }

protected:
    const fmpz* value_;
};

class integer_reference : public integer_const_reference {
public:
    explicit integer_reference(fmpz* value) noexcept
        : integer_const_reference(value)
    {
    }

    integer_reference(const integer_reference&) noexcept = default;

    integer_reference& operator=(const integer_reference& other) noexcept
    {
        fmpz_set(mutable_handle(), other.native_handle());
        return *this;
    }

    integer_reference& operator=(integer_const_reference other) noexcept
    {
        fmpz_set(mutable_handle(), other.native_handle());
        return *this;
    }

    integer_reference& operator+=(integer_const_reference other) noexcept
    {
        fmpz_add(mutable_handle(), mutable_handle(), other.native_handle());
        return *this;
    }

    integer_reference& operator-=(integer_const_reference other) noexcept
    {
        fmpz_sub(mutable_handle(), mutable_handle(), other.native_handle());
        return *this;
    }

    void set_zero() noexcept { fmpz_zero(mutable_handle()); }
    void set_one() noexcept { fmpz_one(mutable_handle()); }
    void negate() noexcept { fmpz_neg(mutable_handle(), mutable_handle()); }
    void set_abs(integer_const_reference value) noexcept { fmpz_abs(mutable_handle(), value.native_handle()); }

    void set_difference(integer_const_reference left, integer_const_reference right) noexcept
    {
        fmpz_sub(mutable_handle(), left.native_handle(), right.native_handle());
    }

    void set_product(integer_const_reference left, integer_const_reference right) noexcept
    {
        fmpz_mul(mutable_handle(), left.native_handle(), right.native_handle());
    }

    void multiply(unsigned long value) noexcept { fmpz_mul_ui(mutable_handle(), mutable_handle(), static_cast<ulong>(value)); }

    void addmul(integer_const_reference left, integer_const_reference right) noexcept
    {
        fmpz_addmul(mutable_handle(), left.native_handle(), right.native_handle());
    }

    void submul(integer_const_reference left, integer_const_reference right) noexcept
    {
        fmpz_submul(mutable_handle(), left.native_handle(), right.native_handle());
    }

    void divide_exact(integer_const_reference divisor) noexcept
    {
        fmpz_divexact(mutable_handle(), mutable_handle(), divisor.native_handle());
    }

    fmpz* native_handle() noexcept { return mutable_handle(); }
    using integer_const_reference::native_handle;

private:
    fmpz* mutable_handle() const noexcept { return const_cast<fmpz*>(value_); }
};

} // namespace detail

/*
 * Owning C++ value wrapper around FLINT's arbitrary-precision fmpz_t integer.
 *
 * Small methods are inline so the wrapper adds no arithmetic layer: optimized code reaches the same FLINT calls with the same
 * operands. Destination-first and in-place operations avoid arbitrary-precision temporaries in exact matrix kernels.
 */
class integer {
public:
    using reference = detail::integer_reference;
    using const_reference = detail::integer_const_reference;

    integer() noexcept { fmpz_init(data_); }

    explicit integer(slong value) noexcept { fmpz_init_set_si(data_, value); }

    explicit integer(const_reference value) noexcept { fmpz_init_set(data_, value.native_handle()); }

    integer(const integer& other) noexcept { fmpz_init_set(data_, other.data_); }

    integer(integer&& other) noexcept
    {
        fmpz_init(data_);
        fmpz_swap(data_, other.data_);
    }

    ~integer() noexcept { fmpz_clear(data_); }

    integer& operator=(const integer& other) noexcept
    {
        if (this != &other) fmpz_set(data_, other.data_);
        return *this;
    }

    integer& operator=(integer&& other) noexcept
    {
        if (this != &other) fmpz_swap(data_, other.data_);
        return *this;
    }

    integer& operator=(const_reference other) noexcept
    {
        fmpz_set(data_, other.native_handle());
        return *this;
    }

    integer& operator+=(const_reference other) noexcept
    {
        ref() += other;
        return *this;
    }

    integer& operator-=(const_reference other) noexcept
    {
        ref() -= other;
        return *this;
    }

    void set_zero() noexcept { ref().set_zero(); }
    void set_one() noexcept { ref().set_one(); }
    void negate() noexcept { ref().negate(); }
    void set_abs(const_reference value) noexcept { ref().set_abs(value); }
    void set_difference(const_reference left, const_reference right) noexcept { ref().set_difference(left, right); }
    void set_product(const_reference left, const_reference right) noexcept { ref().set_product(left, right); }
    void multiply(unsigned long value) noexcept { ref().multiply(value); }
    void addmul(const_reference left, const_reference right) noexcept { ref().addmul(left, right); }
    void submul(const_reference left, const_reference right) noexcept { ref().submul(left, right); }
    void divide_exact(const_reference divisor) noexcept { ref().divide_exact(divisor); }

    int sign() const noexcept { return cref().sign(); }
    bool is_zero() const noexcept { return cref().is_zero(); }
    bool is_one() const noexcept { return cref().is_one(); }
    int compare(const_reference other) const noexcept { return cref().compare(other); }
    int compare_abs(const_reference other) const noexcept { return cref().compare_abs(other); }
    double to_dbl_2exp(slong& exponent) const noexcept { return cref().to_dbl_2exp(exponent); }

    reference ref() noexcept { return reference(data_); }
    const_reference cref() const noexcept { return const_reference(data_); }
    operator const_reference() const noexcept { return cref(); }

    fmpz* native_handle() noexcept { return data_; }
    const fmpz* native_handle() const noexcept { return data_; }

    void swap(integer& other) noexcept { fmpz_swap(data_, other.data_); }

private:
    fmpz_t data_;
};

inline void swap(integer& left, integer& right) noexcept
{
    left.swap(right);
}

} // namespace coposit
