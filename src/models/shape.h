#pragma once

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>
#include "math/native.h"
#include "resources/binary.h"

namespace tfx::models {

// Values are the original disk opcodes; no rewritten native function pointers
// are needed to retain the instruction addresses and relative branches
struct shape_instruction {
  unsigned int address;
  uint16_t opcode;
  std::vector<uint16_t> operands;
};
enum class detail_kind : uint16_t { polygon, reserved, pixel_block, pixel };
struct shape_detail {
  unsigned int distance;
  detail_kind kind;
  unsigned int offset;
  uint8_t colour;
};
struct shape {
  std::map<unsigned int, shape_instruction> instructions;
  std::vector<unsigned int> levels;
  std::vector<shape_detail> details;
  std::string texture_name;
  int16_t sort_depth_bias;
  int16_t minimum_draw_distance;
  bool has_surfaces{false};
  shape(std::span<uint8_t const> source);
  shape_detail const *detail_for_distance(int16_t distance) const;
};

enum class primitive_kind : uint32_t { polygon, line, point };
enum class texture_source { global, model };
enum class palette_source { distance, fixed, face_depth, direct };
struct texture_binding {
  texture_source source;
  unsigned int slot;
  std::string name;
};
struct model_vertex {
  vec3d position;
  vec2d uv;
  double shade{-1};
  // World units per native camera unit; Q15 model products enter Q16 projection.
  double projection_scale{1};
};
struct primitive {
  primitive_kind kind;
  std::vector<model_vertex> vertices;
  unsigned int colour;
  int mode{-1};
  palette_source palette{palette_source::distance};
  // Signed camera high-word average captured by opcode 118, before clipping.
  int16_t palette_depth{0};
  std::optional<texture_binding> texture{std::nullopt};
  double radius{0};
  unsigned int pixel_size{0};
  bool rounded{false};
  unsigned int source_address{0};
  std::optional<unsigned int> attachment_slot;
  bool model_proxy{false}; // A whole distant object, rather than an authored light.
};
enum class facing_test { branch, polygon };

// Native interpretation emits camera vertices directly. Inspection instead
// retains continuous local geometry for its separate interactive transform.
struct shape_camera {
  mat3<int32_t> basis;
  vec3<int32_t> origin;
  unsigned int projection_shift{8}; // Normal 256-pixel focal length; the missile-nose aperture uses 9.
};

struct shape_context {
  std::array<uint8_t, 256> parameters{0};
  uint16_t status{0};
  int16_t distance{0};
  unsigned int level{0};
  std::optional<unsigned int> entry_address{};
  uint8_t initial_state{0};
  double reference_scale{1};
  bool facing{true};
  std::optional<shape_camera> camera{};
  std::function<vec2d(vec3d const&, double)> project{};
  std::function<double(vec3d const&)> camera_depth{};
  std::function<std::optional<bool>(std::span<model_vertex const>,facing_test)> face_visibility{};
  math::jitter_state *jitter{nullptr};
  std::span<shape const *const> attachments{};
};
struct shape_geometry {
  std::vector<primitive> primitives;
  std::set<std::string> warnings;
  unsigned int steps{0};
  bool projection_aborted{false}; // Native divide recovery returns from this model, retaining earlier draws.
};

bool condition(uint16_t opcode, uint16_t operand, uint8_t state, int16_t distance, uint16_t status);
shape_geometry interpret(shape const &program, shape_context const &context);

} // namespace tfx::models
