#pragma once

#include "presentation/program.h"

namespace tfx::presentation {

unsigned int constexpr saved_variable_slots{30}, saved_string_bytes{21}, saved_return_limit{100};
enum class save_mode : uint16_t {
  without_resume,
  previous_statement,
  current_statement,
  following_statement
};
struct saved_state {
  variables values;
  // The native file writes thirty slots even though retail tfx.par declares only twenty-seven.
  std::vector<uint32_t> reserved;
  // These name the original call statements; the native return loop advances past each one.
  std::vector<location> callers;
  std::optional<location> resume;
};
saved_state decode_save(program const &schema, std::span<uint8_t const> source);
resources::bytes encode_save(program const &schema, saved_state const &state);

} // namespace tfx::presentation
