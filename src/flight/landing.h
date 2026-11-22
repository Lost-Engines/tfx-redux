#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>
#include "math/native.h"
#include "flight/aircraft.h"

namespace tfx::flight {

struct landing_port {
  vec2<int16_t> offset{0,0};
  math::angle heading{0};
};
struct landing_surface {
  vec3<int32_t> position_q8{0,0,0};
  landing_port port;
  bool destroyed{false};
};
enum class landing_radio { carrier_destroyed, no_runway, engaged, approach_speed };
enum class approach_height : uint8_t { high, aligned, low };
struct landing_state {
  bool enabled{false};
  bool carrier{false};
  bool beacon{false};
  vec2<int16_t> cell{0,0};
  landing_port port;
  vec2<int32_t> beacon_position{0,0};
  vec2<math::angle> bearing{0,0};
  vec2<int16_t> correction{0,0};
  int32_t pitch_error{0};
  approach_height height{approach_height::high};
};
struct landing_controls {
  uint16_t configuration;
  double response;
  double pitch_response;
  double roll_response;
  double yaw_response;
};
struct landing_input {
  player_aircraft aircraft{player_aircraft::eurofighter};
  bool inhibited{false};
  bool crashed{false};
  bool ejected{false};
  bool damaged{false};
  bool realistic{false};
};
struct landing_selection {
  bool navigation{false};
  bool afterburner_stop_sample{false};
  std::optional<uint8_t> music_cue{};
  std::optional<landing_controls> controls{};
  std::optional<unsigned int> waypoint{};
  std::vector<landing_radio> messages{};
};
struct landing_projection {
  bool valid{false};
  vec2<int16_t> point{-1,-1};
};
struct landing_cue {
  bool beacon{false};
  int heading{0};
  landing_projection projected{};
  vec2<int16_t> correction{0,0};
  std::optional<approach_height> notice{};
  bool speed_request{false};
};
class landing_system {
private:
  landing_state state;
  void prepare_beacon();

public:
  explicit landing_system(landing_state initial = {});
  landing_state const &value() const;
  void reset();
  void disengage();
  // Carriers are supplied in the original descending allied surface-slot order.
  landing_selection toggle(landing_input const &input, std::span<vec3<int32_t> const> route,
    std::function<std::optional<landing_port>(vec2<int16_t> const&)> const &terrain,
    std::span<landing_surface const> carriers);
  // Native 0xa0bc0: select even when ILS is already enabled (Alt+L).
  landing_selection select_runway(std::span<vec3<int32_t> const> route,
    std::function<std::optional<landing_port>(vec2<int16_t> const&)> const &terrain,
    std::span<landing_surface const> carriers);
  landing_cue advance(vec3<int32_t> const &player, bool speed_request,
    std::function<landing_projection(vec3<int32_t> const&)> const &project);
};

} // namespace tfx::flight
