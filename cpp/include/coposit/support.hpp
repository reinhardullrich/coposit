#pragma once

#include <coposit/bitset.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit {

class support_context;

/** One support mask. Its interpretation belongs to the matrix-wide support_context. */
class support {
    friend class support_context;

public:
    support(const support&) = delete;
    support& operator=(const support&) = delete;

    support(support&& other) noexcept : storage_(std::exchange(other.storage_, 0)) {}

    support& operator=(support&& other) noexcept
    {
        storage_ = std::exchange(other.storage_, 0);
        return *this;
    }

private:
    explicit support(uint64_t storage) noexcept : storage_(storage) {}

    uint64_t storage_;
};

static_assert(sizeof(support) == sizeof(uint64_t));
static_assert(sizeof(std::uintptr_t) <= sizeof(uint64_t));

/**
 * Matrix-wide support representation and storage.
 *
 * Through dimension 64, a support stores its bits directly. Above dimension 64, it stores a pointer to an exact-sized word array
 * owned by this context. The dimension decision and allocation bookkeeping therefore exist once per analyzer, not in every mask.
 */
class support_context {
public:
    explicit support_context(size_t dimension)
        : dimension_(dimension)
        , word_count_(dimension / 64 + static_cast<size_t>(dimension % 64 != 0))
        , last_word_mask_(make_last_word_mask(dimension))
    {
        if (dimension == 0) throw std::invalid_argument("support_context dimension must be positive");
    }

    support_context(const support_context&) = delete;
    support_context& operator=(const support_context&) = delete;
    support_context(support_context&&) = delete;
    support_context& operator=(support_context&&) = delete;

    size_t dimension() const noexcept { return dimension_; }
    bool is_small() const noexcept { return dimension_ <= support_detail::kMaxBitsetDimension; }

    uint64_t small_bits(const support& value) const noexcept
    {
        assert(is_small());
        return value.storage_;
    }

    support make()
    {
        if (is_small()) return support(0);

        if (released_ != nullptr) {
            uint64_t* storage = released_;
            released_ = reinterpret_cast<uint64_t*>(static_cast<std::uintptr_t>(storage[0]));
            std::fill_n(storage, word_count_, uint64_t{0});
            return support(static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(storage)));
        }

        auto words = std::make_unique<uint64_t[]>(word_count_);
        const auto address = reinterpret_cast<std::uintptr_t>(words.get());
        allocations_.push_back(std::move(words));
        return support(static_cast<uint64_t>(address));
    }

    support clone(const support& source)
    {
        support result = make();
        copy(result, source);
        return result;
    }

    // Return a no-longer-needed large slot for reuse. The moved-from handle must not be used again.
    void release(support&& value) noexcept
    {
        if (is_small()) {
            value.storage_ = 0;
            return;
        }
        if (value.storage_ == 0) return;
        uint64_t* storage = words(value);
        storage[0] = static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(released_));
        released_ = storage;
        value.storage_ = 0;
    }

    void copy(support& destination, const support& source) const noexcept
    {
        if (is_small()) destination.storage_ = source.storage_;
        else std::copy_n(words(source), word_count_, words(destination));
    }

    void swap(support& left, support& right) const noexcept { std::swap(left.storage_, right.storage_); }

    bool empty(const support& value) const noexcept
    {
        if (is_small()) return value.storage_ == 0;
        for (size_t i = 0; i < word_count_; ++i)
            if (words(value)[i] != 0) return false;
        return true;
    }

    void clear(support& value) const noexcept
    {
        if (is_small()) value.storage_ = 0;
        else std::fill_n(words(value), word_count_, uint64_t{0});
    }

    void set_all(support& value) const noexcept
    {
        if (is_small()) {
            value.storage_ = support_detail::set_all_n_bits(dimension_);
            return;
        }
        std::fill_n(words(value), word_count_, ~uint64_t{0});
        words(value)[word_count_ - 1] &= last_word_mask_;
    }

    bool contains(const support& value, size_t position) const noexcept
    {
        assert(position < dimension_);
        if (is_small()) return (value.storage_ & (uint64_t{1} << position)) != 0;
        return (words(value)[position / 64] & (uint64_t{1} << (position % 64))) != 0;
    }

    void set(support& value, size_t position) const noexcept
    {
        assert(position < dimension_);
        if (is_small()) value.storage_ |= uint64_t{1} << position;
        else words(value)[position / 64] |= uint64_t{1} << (position % 64);
    }

    void reset(support& value, size_t position) const noexcept
    {
        assert(position < dimension_);
        if (is_small()) value.storage_ &= ~(uint64_t{1} << position);
        else words(value)[position / 64] &= ~(uint64_t{1} << (position % 64));
    }

    void add(support& destination, const support& source) const noexcept
    {
        if (is_small()) {
            destination.storage_ |= source.storage_;
            return;
        }
        for (size_t i = 0; i < word_count_; ++i) words(destination)[i] |= words(source)[i];
    }

    void intersect(support& destination, const support& source) const noexcept
    {
        if (is_small()) {
            destination.storage_ &= source.storage_;
            return;
        }
        for (size_t i = 0; i < word_count_; ++i) words(destination)[i] &= words(source)[i];
    }

    void subtract(support& destination, const support& source) const noexcept
    {
        if (is_small()) {
            destination.storage_ &= ~source.storage_;
            return;
        }
        for (size_t i = 0; i < word_count_; ++i) words(destination)[i] &= ~words(source)[i];
    }

    bool is_subset_of(const support& subset, const support& superset) const noexcept
    {
        if (is_small()) return (subset.storage_ & ~superset.storage_) == 0;
        for (size_t i = 0; i < word_count_; ++i)
            if ((words(subset)[i] & ~words(superset)[i]) != 0) return false;
        return true;
    }

    bool equal(const support& left, const support& right) const noexcept
    {
        if (is_small()) return left.storage_ == right.storage_;
        return std::equal(words(left), words(left) + word_count_, words(right));
    }

    bool less(const support& left, const support& right) const noexcept
    {
        if (is_small()) return left.storage_ < right.storage_;
        for (size_t i = word_count_; i > 0; --i) {
            if (words(left)[i - 1] != words(right)[i - 1]) return words(left)[i - 1] < words(right)[i - 1];
        }
        return false;
    }

    size_t count(const support& value) const noexcept
    {
        if (is_small()) return support_detail::popcount64(value.storage_);
        size_t result = 0;
        for (size_t i = 0; i < word_count_; ++i) result += support_detail::popcount64(words(value)[i]);
        return result;
    }

    // Caller precondition: value is nonempty.
    size_t first(const support& value) const noexcept
    {
        if (is_small()) return support_detail::ctz64(value.storage_);
        for (size_t i = 0; i < word_count_; ++i)
            if (words(value)[i] != 0) return i * 64 + support_detail::ctz64(words(value)[i]);
        assert(false && "support_context::first requires a nonempty support");
        return dimension_;
    }

    void extract_set_indices(const support& value, std::vector<size_t>& indices) const
    {
        indices.clear();
        if (is_small()) {
            uint64_t word = value.storage_;
            while (word != 0) {
                indices.push_back(support_detail::ctz64(word));
                word &= word - 1;
            }
            return;
        }

        for (size_t word_index = 0; word_index < word_count_; ++word_index) {
            uint64_t word = words(value)[word_index];
            while (word != 0) {
                indices.push_back(word_index * 64 + support_detail::ctz64(word));
                word &= word - 1;
            }
        }
    }

    void extract_unset_indices(const support& value, std::vector<size_t>& indices) const
    {
        indices.clear();
        if (is_small()) {
            uint64_t word = ~value.storage_ & support_detail::set_all_n_bits(dimension_);
            while (word != 0) {
                indices.push_back(support_detail::ctz64(word));
                word &= word - 1;
            }
            return;
        }

        for (size_t word_index = 0; word_index < word_count_; ++word_index) {
            uint64_t word = ~words(value)[word_index];
            if (word_index + 1 == word_count_) word &= last_word_mask_;
            while (word != 0) {
                indices.push_back(word_index * 64 + support_detail::ctz64(word));
                word &= word - 1;
            }
        }
    }

    void rotate_one_right(support& value) const noexcept
    {
        if (is_small()) {
            value.storage_ = support_detail::rot_one_right(value.storage_, dimension_);
            return;
        }

        uint64_t* value_words = words(value);
        const bool wrap = (value_words[0] & uint64_t{1}) != 0;
        for (size_t i = 0; i + 1 < word_count_; ++i)
            value_words[i] = (value_words[i] >> 1) | (value_words[i + 1] << 63);
        value_words[word_count_ - 1] >>= 1;
        if (wrap) value_words[(dimension_ - 1) / 64] |= uint64_t{1} << ((dimension_ - 1) % 64);
    }

    void reflect(support& value) const noexcept
    {
        if (is_small()) {
            value.storage_ = support_detail::reflect(value.storage_, dimension_);
            return;
        }

        uint64_t* value_words = words(value);
        size_t left = 0;
        size_t right = word_count_ - 1;
        while (left < right) {
            const uint64_t low = support_detail::reverse_bits(value_words[left]);
            value_words[left++] = support_detail::reverse_bits(value_words[right]);
            value_words[right--] = low;
        }
        if (left == right) value_words[left] = support_detail::reverse_bits(value_words[left]);

        const size_t used_bits = dimension_ % 64;
        if (used_bits == 0) return;

        const size_t unused_bits = 64 - used_bits;
        for (size_t i = 0; i + 1 < word_count_; ++i)
            value_words[i] = (value_words[i] >> unused_bits) | (value_words[i + 1] << used_bits);
        value_words[word_count_ - 1] >>= unused_bits;
    }

private:
    friend struct support_context_test_access;

    static uint64_t make_last_word_mask(size_t dimension) noexcept
    {
        const size_t used_bits = dimension % 64;
        return used_bits == 0 ? ~uint64_t{0} : (uint64_t{1} << used_bits) - 1;
    }

    static uint64_t* words(support& value) noexcept
    {
        return reinterpret_cast<uint64_t*>(static_cast<std::uintptr_t>(value.storage_));
    }

    static const uint64_t* words(const support& value) noexcept
    {
        return reinterpret_cast<const uint64_t*>(static_cast<std::uintptr_t>(value.storage_));
    }

    size_t dimension_;
    size_t word_count_;
    uint64_t last_word_mask_;
    std::vector<std::unique_ptr<uint64_t[]>> allocations_;
    uint64_t* released_ = nullptr;
};

struct support_less {
    const support_context* context = nullptr;

    bool operator()(const support& left, const support& right) const noexcept
    {
        assert(context != nullptr);
        return context->less(left, right);
    }
};

} // namespace coposit
