#pragma once

#include <coposit/support.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace coposit {

class nbc_upward_supports {
public:
    enum class enumeration_result { exhausted, stopped };
    using visitor = bool (*)(void*, const std::vector<size_t>&);

    explicit nbc_upward_supports(size_t dimension);
    ~nbc_upward_supports();

    nbc_upward_supports(const nbc_upward_supports&) = delete;
    nbc_upward_supports& operator=(const nbc_upward_supports&) = delete;

    void add_interval(const support& lower, const support& upper);
    void add_pair_upward_closure(size_t first, size_t second);
    void add_upward_closure(const std::vector<size_t>& indices);

    void start_cardinality(size_t cardinality, bool high_frontier = false);
    bool take_first(std::vector<size_t>& indices, bool high_frontier = false);
    enumeration_result enumerate_cardinality(size_t cardinality, void* state, visitor visit);
    void commit_layer(size_t completed_cardinality);
    void commit_frontiers(size_t first_remaining_cardinality, size_t last_remaining_cardinality);

    size_t interval_count() const noexcept;
    bool all_future_covered() const noexcept;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace coposit
