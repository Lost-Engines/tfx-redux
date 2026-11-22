#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include "math/native.h"

namespace tfx::world {

unsigned int constexpr part_parameters{8};
struct model_part {
  std::string name;
  vec3<math::angle> rotation;
  std::array<uint8_t, part_parameters> parameters{0};
  uint16_t model_index{65535};
  uint8_t rotation_mode{0};
};
enum class part_position {
  assembly_origin,
  integer_anchor,
  fixed_anchor
};
enum class part_orientation {
  world_axes,
  terrain_axes
};
enum class part_delivery {
  queued,
  immediate
};
enum class part_draw_kind {
  model,
  ground_shadow
};
struct part_draw {
  unsigned int slot;
  vec3d anchor;
  part_draw_kind kind{part_draw_kind::model};
  part_position placement{part_position::assembly_origin};
  part_orientation orientation{part_orientation::terrain_axes};
  part_delivery delivery{part_delivery::queued};
  model_part part;
};
struct parameter_stream {
  unsigned int part;
  unsigned int parameter;
  std::vector<uint16_t> words;
};
struct definition {
  std::vector<model_part> parts;
  std::vector<part_draw> draws;
  std::vector<parameter_stream> streams;
  uint16_t flags;
  int16_t bounding_radius;
  unsigned int custom_bytes{0};
  std::vector<uint8_t> bytecode;
  std::vector<unsigned int> entries;
  std::vector<vec3d> anchors;
  std::shared_ptr<std::vector<std::string> const> names;
  definition(std::span<uint8_t const> source, std::span<std::string const> names);
  definition(std::span<uint8_t const> source, std::shared_ptr<std::vector<std::string> const> names);
};

unsigned int instruction_operands(unsigned int opcode, std::span<uint8_t const> bytes, unsigned int arguments);
std::vector<std::string> model_names(std::span<uint8_t const> source);

} // namespace tfx::world
