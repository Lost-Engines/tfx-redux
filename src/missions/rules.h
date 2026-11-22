#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tfx::missions {

using variables = std::map<std::string, int32_t, std::less<>>;
enum class target_life {
  dead,
  alive
};
struct result_state {
  int32_t personal_air_score{0};
  int32_t enemy_air_score{0};
  int32_t friendly_air_score{0};
  int32_t enemy_aircraft_down{0};
  int32_t friendly_aircraft_down{0};
  int32_t completed{0};
  int32_t targets_destroyed{0};
  int32_t enemy_mobile_down{0};
  int32_t credited_air_kills{0};
  int32_t credited_ground_kills{0};
  int16_t credited_targets{0};
  std::vector<target_life> targets{};
};

struct operand {
  std::variant<int32_t, std::string> value;
};
enum class comparison {
  less,
  greater,
  equal
};
struct numeric_test {
  operand first;
  comparison operation;
  operand second;
};
struct target_test {
  unsigned int index;
  target_life expected;
};
struct predicate {
  std::variant<numeric_test, target_test> value;
};
enum class logical_operation {
  conjunction,
  disjunction
};
struct condition {
  std::vector<predicate> tests;
  std::vector<logical_operation> operations;
};
enum class assignment_operation {
  set,
  add,
  subtract
};
struct assignment {
  std::string variable;
  assignment_operation operation;
  operand value;
};
struct return_value {
  int32_t value;
};
struct statement;
struct conditional {
  condition test;
  bool braced;                                                                 // native skip cleanup depends on the authored opening brace
  std::vector<statement> when_true;
  std::vector<statement> when_false;
};
struct statement {
  std::variant<assignment, return_value, conditional> value;
};

class finish_program {
private:
  std::vector<statement> statements;

public:
  explicit finish_program(std::string_view source);
  void execute(result_state const &results, variables &campaign) const;
};

} // namespace tfx::missions
