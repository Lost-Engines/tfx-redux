#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>
#include "math/native.h"
#include "resources/binary.h"

namespace tfx::presentation {

enum class node_kind : uint8_t {
  animation,
  image,
  text,
  branch,
  return_from_call,
  launch,
  condition,
  quit,
  options,
  planner,
  assign,
  audio,
  reset_skip,
  load_parser,
  ultra_choice
};
struct variable {
  std::string name;
  bool text;
};
struct location {
  unsigned int function;
  unsigned int statement;
  bool operator==(location const &) const = default;
};
enum class text_alignment : uint8_t {
  left,
  right,
  centred
};
struct text_block {
  vec2<int16_t> position;
  text_alignment alignment;
  bool shadow;
  bool reveal;
  uint8_t font;
  uint8_t colour;
  std::string text;
};
struct clip {
  std::string resource{};
  uint8_t fade_in{0};
  uint8_t fade_out{0};
  uint16_t frame_delay{0};
  int16_t wait_ticks{0};
  bool play_until_end{false};
  bool default_text_palette{false};
  std::optional<text_block> text{};
};
struct branch {
  bool conditional;
  unsigned int variable;
  uint8_t comparison;
  uint32_t value;
  bool subroutine;
  location target;
};
enum class assignment_operation : uint8_t {
  set,
  add,
  subtract
};
struct assignment {
  unsigned int variable;
  assignment_operation operation;
  uint32_t value;
};
struct host_action {
  std::string resource;
  uint16_t mode{0};
};
struct audio_request {
  uint8_t device;
  uint8_t operation;
  int16_t index;
};
struct opaque_action {
  resources::bytes payload;
  std::vector<resources::bytes> buffers{};
};
using action = std::variant<std::monostate, clip, branch, assignment, host_action, audio_request, opaque_action>;
struct statement {
  unsigned int source_offset;
  node_kind kind;
  action data;
};
struct function {
  std::string name;
  std::vector<statement> statements;
};
struct program {
  std::vector<variable> variables;
  std::vector<function> functions;
  explicit program(std::span<uint8_t const> source);
};
using variable_value = std::variant<uint32_t, std::string>;
using variables = std::map<std::string, variable_value, std::less<>>;
bool branch_taken(branch const &branch, program const &source, variables const &state);
void assign(assignment const &assignment, program const &source, variables &state);

} // namespace tfx::presentation
