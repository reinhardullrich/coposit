#pragma once

#ifdef COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::cbdd_dickinson_improved_1_diagnostics {

struct event {
  std::string_view name;
  size_t value = 0;

  bool operator==(const event &other) const noexcept {
    return name == other.name && value == other.value;
  }
};

inline thread_local std::vector<event> events;

inline void clear() { events.clear(); }
inline void record(std::string_view name, size_t value) {
  events.push_back({name, value});
}

} // namespace coposit::cbdd_dickinson_improved_1_diagnostics

#define COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS(...)                     \
  ::coposit::cbdd_dickinson_improved_1_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS(...) ((void)0)

#endif
