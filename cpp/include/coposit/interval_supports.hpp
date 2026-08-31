#pragma once

#include <coposit/support.hpp>

#include <cstddef>
#include <vector>

namespace coposit {

/** Enumerates fixed-cardinality supports not covered by stored Boolean intervals [lower, upper]. The context must outlive this object. */
class interval_supports {
public:
    explicit interval_supports(support_context& context);
    ~interval_supports();

    interval_supports(const interval_supports&) = delete;
    interval_supports& operator=(const interval_supports&) = delete;

    void add_interval(const support& lower, const support& upper);

    void start_low_cardinality(size_t cardinality);
    void start_high_cardinality(size_t cardinality);
    bool take_first_low(std::vector<size_t>& indices);
    bool take_first_high(std::vector<size_t>& indices);

    size_t interval_count() const noexcept { return all_covered_ ? 1 : intervals_.size(); }
    bool covers(const support& candidate) const;
    // True when one stored interval contains the complete query interval; this is not a union-coverage test.
    bool covers_interval(const support& lower, const support& upper) const;

private:
    struct interval {
        support lower;
        support upper;
        size_t lower_size;
        size_t upper_size;
        size_t trigger;
    };

    struct prefix {
        support values;
        size_t length;
        size_t selected;
        size_t known_interval_count;
    };

    struct stream {
        bool started = false;
        bool exhausted = false;
        size_t cardinality = 0;
        std::vector<prefix> unexplored;
    };

    bool triggered_interval_covers(size_t trigger, const support& assigned, size_t cardinality) const;
    bool new_interval_covers(const prefix& current, size_t cardinality) const;
    void start_cardinality(stream& selected, size_t cardinality);
    bool take_first(stream& selected, std::vector<size_t>& indices);
    void clear_intervals() noexcept;
    void exhaust_stream(stream& value) noexcept;
    void clear_stream(stream& value) noexcept;

    support_context& context_;
    size_t dimension_;
    std::vector<interval> intervals_;
    std::vector<std::vector<size_t>> intervals_by_trigger_;
    stream low_stream_;
    stream high_stream_;
    bool all_covered_ = false;
};

} // namespace coposit
