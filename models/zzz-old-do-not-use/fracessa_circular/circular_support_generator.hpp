#pragma once

#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace coposit::model::fracessa_circular_detail {

/* FracESSA's direct fixed-density binary bracelet generator, adapted to coposit's runtime-sized support. */
class circular_support_generator {
public:
    explicit circular_support_generator(size_t dimension)
        : dimension_(dimension)
        , current_(dimension)
        , reflected_(dimension)
        , orbit_current_(dimension)
        , positions_(dimension + 2, 0)
        , prefix_density_(dimension + 2, 0)
        , forbidden_by_lowest_(dimension)
    {
    }

    template<class Callback>
    void generate(Callback&& callback)
    {
        for (target_cardinality_ = 1; target_cardinality_ <= dimension_; ++target_cardinality_) {
            timeout_checkpoint();
            activate_pending();
            current_.clear();
            std::fill(positions_.begin(), positions_.end(), size_t{0});
            std::fill(prefix_density_.begin(), prefix_density_.end(), size_t{0});
            emitted_ = false;

            if (target_cardinality_ == 1) {
                set_position(dimension_);
                if (!completes_forbidden(0)) {
                    emitted_ = true;
                    if (!callback(current_, target_cardinality_)) return;
                }
                clear_position(dimension_);
            } else if (target_cardinality_ == dimension_) {
                if (!has_active_forbidden_) {
                    current_.set_all();
                    emitted_ = true;
                    if (!callback(current_, target_cardinality_)) return;
                    current_.clear();
                }
            } else {
                positions_[target_cardinality_] = dimension_;
                prefix_density_[dimension_] = target_cardinality_;
                const size_t first_latest = dimension_ - target_cardinality_ + 1;
                const size_t first_earliest = (dimension_ - 1) / target_cardinality_ + 1;

                for (size_t first = first_latest;; --first) {
                    positions_[1] = first;
                    set_position(first);
                    prefix_density_[first] = 1;
                    const size_t bit = dimension_ - first;
                    if (!completes_forbidden(bit) && !generate_bracelets(1, 1, first - 1, false, callback)) return;
                    clear_position(first);
                    prefix_density_[first] = 0;
                    if (first == first_earliest) break;
                }
            }

            // Every larger support contains one of this cardinality, so no surviving support means the search is complete.
            if (!emitted_) return;
        }
    }

    void add_forbidden_orbit(const support& forbidden)
    {
        reflected_ = forbidden;
        reflected_.reflect();

        bool reflection_is_rotation = false;
        orbit_current_ = forbidden;
        do {
            reflection_is_rotation = reflection_is_rotation || orbit_current_ == reflected_;
            pending_forbidden_.push_back(orbit_current_);
            orbit_current_.rotate_one_right();
        } while (orbit_current_ != forbidden);

        if (!reflection_is_rotation) {
            orbit_current_ = reflected_;
            do {
                pending_forbidden_.push_back(orbit_current_);
                orbit_current_.rotate_one_right();
            } while (orbit_current_ != reflected_);
        }
    }

private:
    void activate_pending()
    {
        if (!pending_forbidden_.empty()) has_active_forbidden_ = true;
        for (support& forbidden : pending_forbidden_) {
            forbidden_by_lowest_[forbidden.lowest_index()].push_back(std::move(forbidden));
        }
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit]) {
            if (forbidden.is_subset_of(current_)) return true;
        }
        return false;
    }

    bool position_is_set(size_t position) const noexcept
    {
        return current_.contains(dimension_ - position);
    }

    void set_position(size_t position) noexcept
    {
        current_.set(dimension_ - position);
    }

    void clear_position(size_t position) noexcept
    {
        current_.reset(dimension_ - position);
    }

    // Return 1 when the current prefix is smaller than its reversal, 0 when equal, and -1 otherwise.
    int check_reverse(size_t end_position) const noexcept
    {
        for (size_t position = positions_[1]; position <= (end_position + 1) / 2; ++position) {
            const bool forward = position_is_set(position);
            const bool reverse = position_is_set(end_position - position + 1);
            if (forward < reverse) return 1;
            if (forward > reverse) return -1;
        }
        return 0;
    }

    bool update_reverse_result(size_t density, size_t palindrome_prefix, bool reverse_smaller) const noexcept
    {
        const size_t latest_position = positions_[density];
        if (latest_position > (dimension_ - palindrome_prefix) / 2 + palindrome_prefix) {
            const size_t mirrored_position = dimension_ - latest_position + palindrome_prefix + 1;
            const size_t mirrored_density = prefix_density_[mirrored_position];
            if (mirrored_density == 0) {
                reverse_smaller = false;
            } else if (mirrored_density < density) {
                const size_t latest_zero_run = latest_position - positions_[density - 1] - 1;
                const size_t mirrored_zero_run = positions_[mirrored_density + 1] - positions_[mirrored_density] - 1;
                if (latest_zero_run > mirrored_zero_run) reverse_smaller = true;
            }
        }
        return reverse_smaller;
    }

    template<class Callback>
    bool emit_final(size_t period_density, size_t palindrome_prefix, bool reverse_smaller, Callback& callback)
    {
        const size_t next_position = (target_cardinality_ / period_density) * positions_[period_density]
                                     + positions_[target_cardinality_ % period_density];
        if (next_position < dimension_ || (next_position == dimension_ && target_cardinality_ % period_density != 0)) return true;

        set_position(dimension_);
        bool keep_going = true;
        if (!completes_forbidden(0)) {
            reverse_smaller = update_reverse_result(target_cardinality_, palindrome_prefix, reverse_smaller);
            if (!reverse_smaller) {
                emitted_ = true;
                keep_going = callback(current_, target_cardinality_);
            }
        }
        clear_position(dimension_);
        return keep_going;
    }

    template<class Callback>
    bool generate_bracelets(size_t density, size_t period_density, size_t palindrome_prefix,
                            bool reverse_smaller, Callback& callback)
    {
        timeout_checkpoint();
        reverse_smaller = update_reverse_result(density, palindrome_prefix, reverse_smaller);

        if (density >= target_cardinality_ - 1) {
            return emit_final(period_density, palindrome_prefix, reverse_smaller, callback);
        }

        size_t tail = dimension_ - (target_cardinality_ - density) + 1;
        const size_t periodic_position = positions_[density + 1 - period_density] + positions_[period_density];
        if (periodic_position <= tail) {
            size_t next_palindrome_prefix = palindrome_prefix;
            bool next_reverse_smaller = reverse_smaller;
            positions_[density + 1] = periodic_position;
            set_position(periodic_position);
            prefix_density_[periodic_position] = density + 1;

            const size_t bit = dimension_ - periodic_position;
            if (!completes_forbidden(bit)) {
                bool recurse = true;
                if (positions_[1] == periodic_position - positions_[density]) {
                    const int reverse_order = check_reverse(periodic_position - 1);
                    if (reverse_order == 0) {
                        next_palindrome_prefix = periodic_position - 1;
                        next_reverse_smaller = false;
                    }
                    recurse = reverse_order != -1;
                }
                if (recurse && !generate_bracelets(
                        density + 1, period_density, next_palindrome_prefix, next_reverse_smaller, callback)) {
                    clear_position(periodic_position);
                    prefix_density_[periodic_position] = 0;
                    return false;
                }
            }

            clear_position(periodic_position);
            prefix_density_[periodic_position] = 0;
            tail = periodic_position - 1;
        }

        for (size_t position = tail; position > positions_[density]; --position) {
            positions_[density + 1] = position;
            set_position(position);
            prefix_density_[position] = density + 1;
            const size_t bit = dimension_ - position;
            if (!completes_forbidden(bit)
                && !generate_bracelets(density + 1, density + 1, palindrome_prefix, reverse_smaller, callback)) {
                clear_position(position);
                prefix_density_[position] = 0;
                return false;
            }
            clear_position(position);
            prefix_density_[position] = 0;
        }
        return true;
    }

    size_t dimension_;
    size_t target_cardinality_ = 0;
    support current_;
    support reflected_;
    support orbit_current_;
    std::vector<size_t> positions_;
    std::vector<size_t> prefix_density_;
    std::vector<std::vector<support>> forbidden_by_lowest_;
    std::vector<support> pending_forbidden_;
    bool has_active_forbidden_ = false;
    bool emitted_ = false;
};

} // namespace coposit::model::fracessa_circular_detail
