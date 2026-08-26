// bitset.hpp
#pragma once

#include <cstdint>
#include <cstddef>

/*
 * A support is the set of pure strategies played with positive probability.
 * Bit i is one exactly when strategy i belongs to that support. This makes set
 * difference, subset tests, and cyclic shifts ordinary integer operations.
 *
 * The complete search may visit 2^n supports, so these small helpers are called
 * millions or billions of times. They are deliberately inline and mostly
 * unchecked; callers must respect the stated dimension and nonzero preconditions.
 */

// Platform-specific intrinsics
#ifdef _MSC_VER
  #include <intrin.h>
#endif

namespace coposit::support_detail {

// Portable popcount wrapper
inline size_t popcount64(uint64_t x) noexcept {
  #ifdef _MSC_VER
    return static_cast<size_t>(_mm_popcnt_u64(x));
  #else
    return static_cast<size_t>(__builtin_popcountll(x));
  #endif
}

// Caller precondition: x != 0. Deliberately unchecked in this hot primitive.
inline size_t ctz64(uint64_t x) noexcept {
  #ifdef _MSC_VER
    unsigned long index;
    _BitScanForward64(&index, x);
    return static_cast<size_t>(index);
  #else
    return static_cast<size_t>(__builtin_ctzll(x));
  #endif
}

// One word stores every support for dimensions 1 through 64; dimension 64 uses all bits. SupportContext uses these primitives as
// the allocation-free hot path for small games.
using bitset = uint64_t;

constexpr size_t kMaxBitsetDimension = 64;

inline bitset set_all_n_bits(size_t n) noexcept {
  return n == 64 ? ~bitset{0} : (bitset{1} << n) - 1;
}

// Shift every strategy index down by one modulo n. Circular-symmetric games
// use this to obtain the other supports in the same rotational orbit.
inline bitset rot_one_right(bitset bits, size_t n) noexcept {
  bitset mask = set_all_n_bits(n);
  bitset low = bits & mask;
  bitset lo = low << (n - 1);
  bitset hi = low >> 1;
  return (hi | lo) & mask;
}

inline bitset reverse_bits(bitset bits) noexcept {
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
  // GCC does not recognize the portable mask sequence as ARM64's single rbit instruction.
  __asm__("rbit %0, %1" : "=r"(bits) : "r"(bits));
  return bits;
#else
  bits = ((bits >> 1) & 0x5555555555555555ULL) | ((bits & 0x5555555555555555ULL) << 1);
  bits = ((bits >> 2) & 0x3333333333333333ULL) | ((bits & 0x3333333333333333ULL) << 2);
  bits = ((bits >> 4) & 0x0f0f0f0f0f0f0f0fULL) | ((bits & 0x0f0f0f0f0f0f0f0fULL) << 4);
  bits = ((bits >> 8) & 0x00ff00ff00ff00ffULL) | ((bits & 0x00ff00ff00ff00ffULL) << 8);
  bits = ((bits >> 16) & 0x0000ffff0000ffffULL) | ((bits & 0x0000ffff0000ffffULL) << 16);
  return (bits >> 32) | (bits << 32);
#endif
}

// Mirror strategy i to n-1-i. Rotations of this result cover every reflection
// axis of a circular support.
inline bitset reflect(bitset bits, size_t n) noexcept {
  if (n == 0) return 0;
  return reverse_bits(bits) >> (64 - n);
}

} // namespace coposit::support_detail
