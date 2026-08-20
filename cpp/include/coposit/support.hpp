#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace coposit {

class support {
public:
  explicit support(size_t dimension)
      : dimension_(dimension),
        words_(dimension / bits_per_word + (dimension % bits_per_word != 0),
               0) {}

  void set(size_t index) noexcept {
    assert(index < dimension_);
    words_[index / bits_per_word] |= word_type{1} << (index % bits_per_word);
  }

  void reset(size_t index) noexcept {
    assert(index < dimension_);
    words_[index / bits_per_word] &= ~(word_type{1} << (index % bits_per_word));
  }

  void clear() noexcept {
    for (word_type &word : words_)
      word = 0;
  }

  void set_all() noexcept {
    for (word_type &word : words_)
      word = ~word_type{0};
    const size_t used_bits = dimension_ % bits_per_word;
    if (used_bits != 0)
      words_.back() &= (word_type{1} << used_bits) - 1;
  }

  size_t dimension() const noexcept { return dimension_; }

  bool empty() const noexcept {
    for (const word_type word : words_) {
      if (word != 0)
        return false;
    }
    return true;
  }

  size_t cardinality() const noexcept {
    size_t result = 0;
    for (const word_type word : words_)
      result += population_count(word);
    return result;
  }

  void add(const support &other) noexcept {
    assert(dimension_ == other.dimension_);
    for (size_t word_index = 0; word_index < words_.size(); ++word_index)
      words_[word_index] |= other.words_[word_index];
  }

  void intersect_with(const support &other) noexcept {
    assert(dimension_ == other.dimension_);
    for (size_t word_index = 0; word_index < words_.size(); ++word_index)
      words_[word_index] &= other.words_[word_index];
  }

  void remove(const support &other) noexcept {
    assert(dimension_ == other.dimension_);
    for (size_t word_index = 0; word_index < words_.size(); ++word_index)
      words_[word_index] &= ~other.words_[word_index];
  }

  void swap(support &other) noexcept {
    assert(dimension_ == other.dimension_);
    words_.swap(other.words_);
  }

  // Move index i to i-1 modulo the support dimension.
  void rotate_one_right() noexcept {
    const bool wrap = (words_.front() & word_type{1}) != 0;
    for (size_t word_index = 0; word_index + 1 < words_.size(); ++word_index) {
      words_[word_index] = (words_[word_index] >> 1) |
                           (words_[word_index + 1] << (bits_per_word - 1));
    }
    words_.back() >>= 1;
    if (wrap)
      set(dimension_ - 1);
  }

  // Mirror index i to dimension-1-i without allocating scratch storage.
  void reflect() noexcept {
    size_t left = 0;
    size_t right = words_.size() - 1;
    while (left < right) {
      const word_type low = reverse_bits(words_[left]);
      words_[left++] = reverse_bits(words_[right]);
      words_[right--] = low;
    }
    if (left == right)
      words_[left] = reverse_bits(words_[left]);

    const size_t used_bits = dimension_ % bits_per_word;
    if (used_bits == 0)
      return;

    const size_t unused_bits = bits_per_word - used_bits;
    for (size_t word_index = 0; word_index + 1 < words_.size(); ++word_index) {
      words_[word_index] = (words_[word_index] >> unused_bits) |
                           (words_[word_index + 1] << used_bits);
    }
    words_.back() >>= unused_bits;
  }

  bool contains(size_t index) const noexcept {
    assert(index < dimension_);
    return (words_[index / bits_per_word] &
            (word_type{1} << (index % bits_per_word))) != 0;
  }

  bool is_subset_of(const support &other) const noexcept {
    assert(dimension_ == other.dimension_);
    for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
      if ((words_[word_index] & ~other.words_[word_index]) != 0)
        return false;
    }
    return true;
  }

  size_t lowest_index() const noexcept {
    for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
      if (words_[word_index] != 0) {
        return word_index * bits_per_word +
               trailing_zero_count(words_[word_index]);
      }
    }
    assert(false);
    return 0;
  }

  void copy_indices_to(std::vector<size_t> &indices) const {
    indices.clear();
    for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
      word_type word = words_[word_index];
      while (word != 0) {
        const size_t bit = trailing_zero_count(word);
        indices.push_back(word_index * bits_per_word + bit);
        word &= word - 1;
      }
    }
  }

  friend bool operator==(const support &left, const support &right) noexcept {
    return left.dimension_ == right.dimension_ && left.words_ == right.words_;
  }

  friend bool operator!=(const support &left, const support &right) noexcept {
    return !(left == right);
  }

  friend bool operator<(const support &left, const support &right) noexcept {
    assert(left.dimension_ == right.dimension_);
    for (size_t word_index = left.words_.size(); word_index > 0; --word_index) {
      if (left.words_[word_index - 1] != right.words_[word_index - 1]) {
        return left.words_[word_index - 1] < right.words_[word_index - 1];
      }
    }
    return false;
  }

private:
  using word_type = std::uint64_t;
  static constexpr size_t bits_per_word = 64;

  static size_t trailing_zero_count(word_type word) noexcept {
#ifdef _MSC_VER
    unsigned long index;
    _BitScanForward64(&index, word);
    return static_cast<size_t>(index);
#else
    return static_cast<size_t>(
        __builtin_ctzll(static_cast<unsigned long long>(word)));
#endif
  }

  static size_t population_count(word_type word) noexcept {
#ifdef _MSC_VER
    return static_cast<size_t>(__popcnt64(word));
#else
    return static_cast<size_t>(
        __builtin_popcountll(static_cast<unsigned long long>(word)));
#endif
  }

  static word_type reverse_bits(word_type word) noexcept {
    word = ((word & 0x5555555555555555ULL) << 1) |
           ((word >> 1) & 0x5555555555555555ULL);
    word = ((word & 0x3333333333333333ULL) << 2) |
           ((word >> 2) & 0x3333333333333333ULL);
    word = ((word & 0x0f0f0f0f0f0f0f0fULL) << 4) |
           ((word >> 4) & 0x0f0f0f0f0f0f0f0fULL);
    word = ((word & 0x00ff00ff00ff00ffULL) << 8) |
           ((word >> 8) & 0x00ff00ff00ff00ffULL);
    word = ((word & 0x0000ffff0000ffffULL) << 16) |
           ((word >> 16) & 0x0000ffff0000ffffULL);
    return (word << 32) | (word >> 32);
  }

  size_t dimension_;
  std::vector<word_type> words_;
};

} // namespace coposit
