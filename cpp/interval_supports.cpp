#include <coposit/interval_supports.hpp>
#include <coposit/timeout.hpp>

#include <stdexcept>
#include <utility>

namespace coposit {

interval_supports::interval_supports(support_context& context)
    : context_(context)
    , dimension_(context.dimension())
    , intervals_by_trigger_(dimension_)
{}

interval_supports::~interval_supports()
{
    clear_stream(low_stream_);
    clear_stream(high_stream_);
    clear_intervals();
}

void interval_supports::add_interval(const support& lower, const support& upper)
{
    if (!context_.is_subset_of(lower, upper)) throw std::invalid_argument("invalid support interval");
    if (all_covered_) return;

    const size_t lower_size = context_.count(lower);
    const size_t upper_size = context_.count(upper);
    if (lower_size == 0 && upper_size == dimension_) {
        clear_intervals();
        all_covered_ = true;
        exhaust_stream(low_stream_);
        exhaust_stream(high_stream_);
        return;
    }

    size_t trigger = dimension_;
    for (size_t index = dimension_; index > 0; --index) {
        if (context_.contains(lower, index - 1) || !context_.contains(upper, index - 1)) {
            trigger = index - 1;
            break;
        }
    }
    if (trigger == dimension_) throw std::logic_error("non-universal support interval has no constrained coordinate");

    support stored_lower = context_.clone(lower);
    support stored_upper = [&] {
        try {
            return context_.clone(upper);
        } catch (...) {
            context_.release(std::move(stored_lower));
            throw;
        }
    }();
    interval added{std::move(stored_lower), std::move(stored_upper), lower_size, upper_size, trigger};
    const size_t interval_index = intervals_.size();
    try {
        intervals_.push_back(std::move(added));
        intervals_by_trigger_[trigger].push_back(interval_index);
    } catch (...) {
        if (intervals_.size() == interval_index) {
            context_.release(std::move(added.lower));
            context_.release(std::move(added.upper));
        } else {
            interval& stored = intervals_.back();
            context_.release(std::move(stored.lower));
            context_.release(std::move(stored.upper));
            intervals_.pop_back();
        }
        throw;
    }
    // ponytail: stable append-only interval IDs keep live stream updates cheap; add batch compaction only if redundant rules measure badly.
}

void interval_supports::start_low_cardinality(size_t cardinality)
{
    start_cardinality(low_stream_, cardinality);
}

void interval_supports::start_high_cardinality(size_t cardinality)
{
    start_cardinality(high_stream_, cardinality);
}

bool interval_supports::take_first_low(std::vector<size_t>& indices)
{
    return take_first(low_stream_, indices);
}

bool interval_supports::take_first_high(std::vector<size_t>& indices)
{
    return take_first(high_stream_, indices);
}

void interval_supports::start_cardinality(stream& selected, size_t cardinality)
{
    if (cardinality > dimension_) throw std::invalid_argument("support cardinality exceeds the dimension");
    if (selected.started && selected.cardinality == cardinality) return;

    clear_stream(selected);
    selected.started = true;
    selected.cardinality = cardinality;
    if (all_covered_) {
        selected.exhausted = true;
        return;
    }
    selected.unexplored.push_back({context_.make(), 0, 0, intervals_.size()});
}

bool interval_supports::take_first(stream& selected, std::vector<size_t>& indices)
{
    if (!selected.started) throw std::logic_error("support cardinality stream was not started");
    if (selected.exhausted || all_covered_) return false;

    while (!selected.unexplored.empty()) {
        timeout_checkpoint();
        prefix current = std::move(selected.unexplored.back());
        selected.unexplored.pop_back();

        const size_t remaining = dimension_ - current.length;
        if (current.selected > selected.cardinality || selected.cardinality - current.selected > remaining) {
            context_.release(std::move(current.values));
            continue;
        }

        if (new_interval_covers(current, selected.cardinality)
            || (current.length != 0 && triggered_interval_covers(current.length - 1, current.values, selected.cardinality))) {
            context_.release(std::move(current.values));
            continue;
        }

        const size_t needed = selected.cardinality - current.selected;
        if (needed == 0 || needed == remaining) {
            bool covered = false;
            for (size_t index = current.length; index < dimension_; ++index) {
                if (needed != 0) context_.set(current.values, index);
                if (triggered_interval_covers(index, current.values, selected.cardinality)) {
                    covered = true;
                    break;
                }
            }

            if (!covered) {
                context_.extract_set_indices(current.values, indices);
                context_.release(std::move(current.values));
                return true;
            }
            context_.release(std::move(current.values));
            continue;
        }

        support excluded = context_.clone(current.values);
        const size_t known_interval_count = intervals_.size();
        selected.unexplored.push_back({std::move(excluded), current.length + 1, current.selected, known_interval_count});
        context_.set(current.values, current.length);
        selected.unexplored.push_back({std::move(current.values), current.length + 1, current.selected + 1, known_interval_count});
    }

    selected.exhausted = true;
    return false;
}

bool interval_supports::covers(const support& candidate) const
{
    if (all_covered_) return true;
    const size_t cardinality = context_.count(candidate);
    size_t checked = 0;
    for (const interval& value : intervals_) {
        if ((++checked & 1023U) == 0) timeout_checkpoint();
        if (value.lower_size > cardinality || value.upper_size < cardinality) continue;
        if (context_.is_subset_of(value.lower, candidate) && context_.is_subset_of(candidate, value.upper)) return true;
    }
    return false;
}

bool interval_supports::covers_interval(const support& lower, const support& upper) const
{
    if (!context_.is_subset_of(lower, upper)) throw std::invalid_argument("invalid support interval");
    if (all_covered_) return true;
    const size_t lower_size = context_.count(lower);
    const size_t upper_size = context_.count(upper);
    size_t checked = 0;
    for (const interval& value : intervals_) {
        if ((++checked & 1023U) == 0) timeout_checkpoint();
        if (value.lower_size > lower_size || value.upper_size < upper_size) continue;
        if (context_.is_subset_of(value.lower, lower) && context_.is_subset_of(upper, value.upper)) return true;
    }
    return false;
}

bool interval_supports::triggered_interval_covers(size_t trigger, const support& assigned, size_t cardinality) const
{
    size_t checked = 0;
    for (const size_t interval_index : intervals_by_trigger_[trigger]) {
        if ((++checked & 1023U) == 0) timeout_checkpoint();
        const interval& value = intervals_[interval_index];
        if (value.lower_size > cardinality || value.upper_size < cardinality) continue;
        if (context_.is_subset_of(value.lower, assigned) && context_.is_subset_of(assigned, value.upper)) return true;
    }
    return false;
}

bool interval_supports::new_interval_covers(const prefix& current, size_t cardinality) const
{
    if (current.length == 0) return false;
    size_t checked = 0;
    for (size_t index = current.known_interval_count; index < intervals_.size(); ++index) {
        if ((++checked & 1023U) == 0) timeout_checkpoint();
        const interval& value = intervals_[index];
        if (value.trigger >= current.length - 1) continue;
        if (value.lower_size > cardinality || value.upper_size < cardinality) continue;
        if (context_.is_subset_of(value.lower, current.values) && context_.is_subset_of(current.values, value.upper)) return true;
    }
    return false;
}

void interval_supports::clear_intervals() noexcept
{
    for (interval& value : intervals_) {
        context_.release(std::move(value.lower));
        context_.release(std::move(value.upper));
    }
    intervals_.clear();
    for (std::vector<size_t>& values : intervals_by_trigger_) values.clear();
}

void interval_supports::exhaust_stream(stream& value) noexcept
{
    for (prefix& current : value.unexplored) context_.release(std::move(current.values));
    value.unexplored.clear();
    value.exhausted = true;
}

void interval_supports::clear_stream(stream& value) noexcept
{
    for (prefix& current : value.unexplored) context_.release(std::move(current.values));
    value.unexplored.clear();
    value.started = false;
    value.exhausted = false;
    value.cardinality = 0;
}

} // namespace coposit
