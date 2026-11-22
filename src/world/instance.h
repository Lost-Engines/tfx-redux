#pragma once

#include <array>
#include <functional>
#include <optional>
#include "world/animation.h"
#include "world/collision.h"
#include "world/definition.h"

namespace tfx::world {

// Native SSD placement is a word origin plus one of four exact quarter turns.
vec3<int16_t> place_instance_point(vec3<int16_t> const &local, vec3<int16_t> const &origin, math::angle heading);
int16_t instance_point_distance(vec3<int16_t> const &local, vec3<int16_t> const &origin, math::angle heading, vec3<int16_t> const &reference);

struct scene_request;
struct instance_context {
  collision_segment movement{};
  bool player_query{false};
  bool surface_enabled{true};
  uint16_t impact_damage{0};
  uint16_t status{0};
  int16_t distance{0};
  std::function<int16_t(vec3d const &)> anchor_distance{};
  std::function<void(uint16_t)> collision_response{};
  std::function<void(int)> damage_response{};
  // Borrowed only during this synchronous execute/animate call.
  std::optional<std::function_ref<void(scene_request const &)>> request_output{};
};
struct scene_request {
  unsigned int opcode;
  std::vector<uint16_t> arguments;
};
struct instance_result {
  std::vector<part_draw> draws;
  std::vector<scene_request> requests;
  unsigned int collisions{0};
  bool volume_hit{false};
  bool ejection_requested{false};
  bool surface_present{false};
  // Opcode 73 writes the shared ground height independently of surface contact.
  std::optional<int16_t> surface_height{};
  int16_t zone{-1};
  int16_t player_zone{0};
  uint16_t message{0};
  std::vector<int> damage_reports;
};
struct part_stream {
  unsigned int source;
  parameter_player player;
};

class instance {
private:
  definition const &source;
  std::vector<std::array<std::optional<part_stream>, part_parameters>> streams;
  void interpret(unsigned int address, instance_context &context, instance_result &result, unsigned int &budget, unsigned int depth, bool &install_streams);

public:
  std::vector<model_part> parts;
  std::vector<uint16_t> custom;
  bool destroyed{false};
  bool modified{false};
  uint8_t category{0};
  uint16_t impact_damage{0};
  uint16_t health{0};

  explicit instance(definition const &source);
  void apply_contact_damage(uint16_t damage);
  int16_t type() const;
  int16_t bounding_radius() const;
  std::vector<vec3<int16_t>> damage_centres() const;
  instance_result execute(unsigned int entry, instance_context const &context = {});
  instance_result animate(instance_context const &context = {});
  std::optional<unsigned int> stream_cursor(unsigned int part, unsigned int parameter) const;
};

} // namespace tfx::world
