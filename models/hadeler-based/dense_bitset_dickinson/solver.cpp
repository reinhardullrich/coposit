#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace coposit::model {

namespace {

constexpr const char* max_n_environment = "COPOSIT_DENSE_BITSET_MAX_N";
constexpr const char* max_gib_environment = "COPOSIT_DENSE_BITSET_MAX_GIB";
constexpr size_t default_max_gib = 1;
constexpr size_t bits_per_word = 64;

size_t parse_positive_size(const char* name, const char* text)
{
    size_t result = 0;
    const char* end = text;
    while (*end != '\0') ++end;
    const auto parsed = std::from_chars(text, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end || result == 0)
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    return result;
}

size_t bitmap_bit_count(size_t dimension)
{
    if (dimension >= std::numeric_limits<size_t>::digits)
        throw std::length_error("dense_bitset_dickinson cannot address 2^n bits for this dimension");
    return size_t{1} << dimension;
}

size_t required_bitmap_bytes(size_t dimension)
{
    const size_t bit_count = bitmap_bit_count(dimension);
    const size_t word_count = bit_count / bits_per_word + (bit_count % bits_per_word != 0);
    return word_count * sizeof(uint64_t);
}

void enforce_bitmap_limit(size_t dimension, size_t bytes)
{
    const char* max_n_text = std::getenv(max_n_environment);
    const char* max_gib_text = std::getenv(max_gib_environment);
    if (max_n_text != nullptr && max_gib_text != nullptr)
        throw std::invalid_argument(std::string("set either ") + max_n_environment + " or " + max_gib_environment + ", not both");

    if (max_n_text != nullptr) {
        const size_t maximum = parse_positive_size(max_n_environment, max_n_text);
        if (dimension > maximum)
            throw std::length_error("dense_bitset_dickinson matrix dimension exceeds " + std::string(max_n_environment));
        return;
    }

    const size_t gib = max_gib_text == nullptr ? default_max_gib : parse_positive_size(max_gib_environment, max_gib_text);
    if (gib > std::numeric_limits<size_t>::max() / (size_t{1} << 30))
        throw std::invalid_argument(std::string(max_gib_environment) + " is too large");
    if (bytes > (gib << 30))
        throw std::length_error("dense_bitset_dickinson bitmap exceeds " + std::string(max_gib_environment));
}

size_t trailing_zero_count(uint64_t word) noexcept
{
    assert(word != 0);
#ifdef _MSC_VER
    unsigned long index;
    _BitScanForward64(&index, word);
    return static_cast<size_t>(index);
#else
    return static_cast<size_t>(__builtin_ctzll(static_cast<unsigned long long>(word)));
#endif
}

class dense_boolean_lattice {
public:
    explicit dense_boolean_lattice(size_t dimension)
        : dimension_(dimension)
        , bit_count_(bitmap_bit_count(dimension))
        , binomial_stride_(dimension + 1)
        , binomial_(binomial_stride_ * binomial_stride_)
        , layer_offsets_(dimension + 2)
    {
        const size_t bytes = required_bitmap_bytes(dimension);
        enforce_bitmap_limit(dimension, bytes);
        covered_.reset(static_cast<uint64_t*>(std::calloc(bytes / sizeof(uint64_t), sizeof(uint64_t))));
        if (covered_ == nullptr) throw std::bad_alloc();
        cover_bit(0); // The empty support is not in Dickinson's P[n].

        binomial(0, 0) = 1;
        for (size_t row = 1; row <= dimension_; ++row) {
            binomial(row, 0) = 1;
            binomial(row, row) = 1;
            for (size_t column = 1; column < row; ++column)
                binomial(row, column) = binomial(row - 1, column - 1) + binomial(row - 1, column);
        }

        for (size_t cardinality = 0; cardinality <= dimension_; ++cardinality)
            layer_offsets_[cardinality + 1] = layer_offsets_[cardinality] + binomial(dimension_, cardinality);
        assert(layer_offsets_[dimension_ + 1] == bit_count_);
    }

    void start_cardinality(size_t cardinality) noexcept
    {
        current_cardinality_ = cardinality;
        cursor_ = layer_offsets_[cardinality];
        layer_end_ = layer_offsets_[cardinality + 1];
    }

    bool take_next(std::vector<size_t>& indices)
    {
        const size_t position = find_next_uncovered_bit(cursor_, layer_end_);
        if (position == layer_end_) return false;
        cursor_ = position + 1;
        unrank(current_cardinality_, position - layer_offsets_[current_cardinality_], indices);
        return true;
    }

    uint64_t clear_interval(uint64_t lower, uint64_t upper)
    {
        assert((lower & ~upper) == 0);
        assert((upper >> dimension_) == 0);
        cleared_now_ = 0;
        clear_interval_from(upper, lower, 0, 0, popcount(upper));
        return cleared_now_;
    }

private:
    struct free_deleter {
        void operator()(uint64_t* pointer) const noexcept { std::free(pointer); }
    };

    size_t& binomial(size_t row, size_t column) noexcept
    {
        return binomial_[row * binomial_stride_ + column];
    }

    size_t binomial(size_t row, size_t column) const noexcept
    {
        return binomial_[row * binomial_stride_ + column];
    }

    static size_t popcount(uint64_t word) noexcept
    {
#ifdef _MSC_VER
        return static_cast<size_t>(__popcnt64(word));
#else
        return static_cast<size_t>(__builtin_popcountll(static_cast<unsigned long long>(word)));
#endif
    }

    size_t find_next_uncovered_bit(size_t begin, size_t end) const noexcept
    {
        if (begin >= end) return end;
        size_t word_index = begin / bits_per_word;
        uint64_t word = ~covered_[word_index] & (~uint64_t{0} << (begin % bits_per_word));
        while (true) {
            if (word != 0) {
                const size_t position = word_index * bits_per_word + trailing_zero_count(word);
                return position < end ? position : end;
            }
            ++word_index;
            if (word_index * bits_per_word >= end) return end;
            word = ~covered_[word_index];
        }
    }

    void unrank(size_t cardinality, size_t rank, std::vector<size_t>& indices) const
    {
        indices.resize(cardinality);
        size_t upper = dimension_;
        for (size_t order = cardinality; order > 0; --order) {
            size_t value = upper - 1;
            while (binomial(value, order) > rank) --value;
            indices[order - 1] = value;
            rank -= binomial(value, order);
            upper = value;
        }
        assert(rank == 0);
    }

    void clear_interval_from(
        uint64_t remaining_upper, uint64_t remaining_lower, size_t selected, size_t rank, size_t remaining_count)
    {
        if (selected + remaining_count < current_cardinality_) return;

        while (remaining_upper != 0) {
            const size_t index = trailing_zero_count(remaining_upper);
            const uint64_t bit = uint64_t{1} << index;
            remaining_upper &= remaining_upper - 1;
            --remaining_count;
            if ((remaining_lower & bit) != 0) {
                remaining_lower ^= bit;
            } else {
                clear_interval_from(remaining_upper, remaining_lower, selected, rank, remaining_count);
            }
            ++selected;
            rank += binomial(index, selected);
        }

        const size_t position = layer_offsets_[selected] + rank;
        if (position >= cursor_ && cover_bit(position)) ++cleared_now_;
        if ((++clear_operations_ & 4095U) == 0) timeout_checkpoint();
    }

    bool cover_bit(size_t position) noexcept
    {
        assert(position < bit_count_);
        uint64_t& word = covered_[position / bits_per_word];
        const uint64_t mask = uint64_t{1} << (position % bits_per_word);
        const bool was_uncovered = (word & mask) == 0;
        word |= mask;
        return was_uncovered;
    }

    const size_t dimension_;
    const size_t bit_count_;
    const size_t binomial_stride_;
    std::vector<size_t> binomial_;
    std::vector<size_t> layer_offsets_;
    std::unique_ptr<uint64_t[], free_deleter> covered_;
    size_t current_cardinality_ = 0;
    size_t cursor_ = 0;
    size_t layer_end_ = 0;
    uint64_t cleared_now_ = 0;
    uint64_t clear_operations_ = 0;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , supports_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        nonzero_locals_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , supports_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
        nonzero_locals_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            supports_.start_cardinality(subset_dimension);
            while (supports_.take_next(indices_)) {
                timeout_checkpoint();
                diagnostics_.visit_support();
                diagnostics_.secondary();
                COPOSIT_DENSE_BITSET_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
                    return false;
                }
            }
        }

        diagnostics_.finish();
        return true;
    }

private:
    bool process_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (!singular) {
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

            factorization_.solve_inplace(solution_, denominator_, principal_);
            assert(denominator_.sign() > 0);
        } else {
            factorization_.one_nullspace_vector(solution_, principal_);
            bool has_positive_entry = false;
            for (size_t row = 0; row < dimension; ++row) has_positive_entry |= solution_(row, 0).sign() > 0;
            if (!has_positive_entry) solution_.negate();
        }

        bool all_nonpositive = true;
        bool all_nonnegative = singular;
        for (size_t row = 0; row < dimension; ++row) {
            all_nonpositive &= solution_(row, 0).sign() <= 0;
            all_nonnegative &= solution_(row, 0).sign() >= 0;
        }
        if (all_nonpositive) return false;
        if (all_nonnegative) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        add_certificate(matrix);
        return true;
    }

    void add_certificate(const matrix_integer& matrix)
    {
        uint64_t lower = 0;
        uint64_t upper = 0;
        nonzero_locals_.clear();
        for (size_t local = 0; local < indices_.size(); ++local) {
            const uint64_t bit = uint64_t{1} << indices_[local];
            // On the principal support, A_I u is either a positive multiple of one or zero, so I is already contained in N_A(u).
            upper |= bit;
            if (!solution_(local, 0).is_zero()) {
                lower |= bit;
                nonzero_locals_.push_back(local);
            }
        }

        size_t upper_size = indices_.size();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            const uint64_t bit = uint64_t{1} << row;
            if ((upper & bit) != 0) continue;

            row_product_.set_zero();
            for (const size_t local : nonzero_locals_)
                row_product_.addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (row_product_.sign() >= 0) {
                upper |= bit;
                ++upper_size;
            }
        }

        const uint64_t cleared = supports_.clear_interval(lower, upper);
        if (cleared > 0) diagnostics_.skip_supports(cleared);
        diagnostics_.certificate(upper_size - nonzero_locals_.size(), upper_size);
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    integer denominator_;
    integer row_product_;
    std::vector<size_t> indices_;
    std::vector<size_t> nonzero_locals_;
    dense_boolean_lattice supports_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    return dickinson_checker(matrix.rows(), mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    copositivity_classification result{true, true};
    if (!dickinson_checker(matrix.rows(), result).check(matrix)) result = {false, false};
    return result;
}

#ifdef COPOSIT_DENSE_BITSET_DICKINSON_TESTING
size_t dense_bitset_required_bytes(size_t dimension)
{
    return required_bitmap_bytes(dimension);
}

uint64_t dense_bitset_future_clear_count(
    size_t dimension, size_t cardinality, size_t visited, uint64_t lower, uint64_t upper)
{
    dense_boolean_lattice lattice(dimension);
    lattice.start_cardinality(cardinality);
    std::vector<size_t> indices;
    for (size_t ordinal = 0; ordinal < visited; ++ordinal) {
        const bool found = lattice.take_next(indices);
        assert(found);
        (void)found;
    }
    return lattice.clear_interval(lower, upper);
}

std::vector<uint64_t> dense_bitset_remaining_masks(
    size_t dimension, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    dense_boolean_lattice lattice(dimension);
    for (const auto& interval : intervals) lattice.clear_interval(interval.first, interval.second);

    std::vector<uint64_t> result;
    std::vector<size_t> indices;
    for (size_t cardinality = 1; cardinality <= dimension; ++cardinality) {
        lattice.start_cardinality(cardinality);
        while (lattice.take_next(indices)) {
            uint64_t mask = 0;
            for (const size_t index : indices) mask |= uint64_t{1} << index;
            result.push_back(mask);
        }
    }
    return result;
}
#endif

} // namespace coposit::model
