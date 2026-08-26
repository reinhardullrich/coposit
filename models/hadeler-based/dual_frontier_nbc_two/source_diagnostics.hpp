#pragma once

#ifdef COPOSIT_DUAL_FRONTIER_NBC_TWO_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::dual_frontier_nbc_two_diagnostics {

struct event {
  std::string_view name;
  size_t cardinality = 0;

  bool operator==(const event &other) const noexcept {
    return name == other.name && cardinality == other.cardinality;
  }
};

inline thread_local std::vector<event> events;

inline void clear() { events.clear(); }
inline void record(std::string_view name, size_t cardinality) {
  events.push_back({name, cardinality});
}

} // namespace coposit::dual_frontier_nbc_two_diagnostics

#define COPOSIT_DUAL_FRONTIER_NBC_TWO_DIAGNOSTICS(...)                         \
  ::coposit::dual_frontier_nbc_two_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_DUAL_FRONTIER_NBC_TWO_DIAGNOSTICS(...) ((void)0)

#endif
