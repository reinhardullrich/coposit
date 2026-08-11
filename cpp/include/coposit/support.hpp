#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace coposit {

class support {
public:
    explicit support(size_t dimension)
        : dimension_(dimension)
        , words_(dimension / bits_per_word + (dimension % bits_per_word != 0), 0)
    {
    }

    void set(size_t index) noexcept
    {
        assert(index < dimension_);
        words_[index / bits_per_word] |= word_type{1} << (index % bits_per_word);
    }

    void reset(size_t index) noexcept
    {
        assert(index < dimension_);
        words_[index / bits_per_word] &= ~(word_type{1} << (index % bits_per_word));
    }

    void clear() noexcept
    {
        for (word_type& word : words_) word = 0;
    }

    bool empty() const noexcept
    {
        for (const word_type word : words_) {
            if (word != 0) return false;
        }
        return true;
    }

    void add(const support& other) noexcept
    {
        assert(dimension_ == other.dimension_);
        for (size_t word_index = 0; word_index < words_.size(); ++word_index) words_[word_index] |= other.words_[word_index];
    }

    void intersect_with(const support& other) noexcept
    {
        assert(dimension_ == other.dimension_);
        for (size_t word_index = 0; word_index < words_.size(); ++word_index) words_[word_index] &= other.words_[word_index];
    }

    void remove(const support& other) noexcept
    {
        assert(dimension_ == other.dimension_);
        for (size_t word_index = 0; word_index < words_.size(); ++word_index) words_[word_index] &= ~other.words_[word_index];
    }

    void swap(support& other) noexcept
    {
        assert(dimension_ == other.dimension_);
        words_.swap(other.words_);
    }

    bool contains(size_t index) const noexcept
    {
        assert(index < dimension_);
        return (words_[index / bits_per_word] & (word_type{1} << (index % bits_per_word))) != 0;
    }

    bool is_subset_of(const support& other) const noexcept
    {
        assert(dimension_ == other.dimension_);
        for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
            if ((words_[word_index] & ~other.words_[word_index]) != 0) return false;
        }
        return true;
    }

    size_t lowest_index() const noexcept
    {
        for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
            if (words_[word_index] != 0) {
                return word_index * bits_per_word
                    + static_cast<size_t>(__builtin_ctzll(static_cast<unsigned long long>(words_[word_index])));
            }
        }
        assert(false);
        return 0;
    }

    void copy_indices_to(std::vector<size_t>& indices) const
    {
        indices.clear();
        for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
            word_type word = words_[word_index];
            while (word != 0) {
                const size_t bit = static_cast<size_t>(__builtin_ctzll(static_cast<unsigned long long>(word)));
                indices.push_back(word_index * bits_per_word + bit);
                word &= word - 1;
            }
        }
    }

private:
    using word_type = std::uint64_t;
    static constexpr size_t bits_per_word = 64;

    size_t dimension_;
    std::vector<word_type> words_;
};

} // namespace coposit
