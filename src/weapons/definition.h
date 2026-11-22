#pragma once

#include <array>
#include <string>
#include <vector>
#include "resources/binary.h"

namespace tfx::weapons {

enum class weapon_id : uint8_t {
  cannon,
  sidewinder_s,
  sidewinder_l,
  asraam,
  amraam,
  aaam,
  aa_arm,
  fuel_tank,
  mk82,
  gbu10,
  gbu24,
  gbu16,
  gbu12,
  gbu24_blu,
  cbu55,
  jp233,
  csw,
  durandal,
  maverick,
  agm109,
  harm,
  sidearm,
  harpoon,
  submunition,
  chaff,
  flare,
  cargo
};
unsigned int constexpr weapon_count{27};
uint8_t constexpr cruise_missile_category{11};
enum class weapon_role : uint8_t {
  air,
  ground,
  external_tank = 6
};
enum class flight_stage {
  finished,
  attached,
  falling,
  accelerating,
  coasting,
  guided,
  sea_skimming,
  falling_store,
  retarded_store,
  attached_dispenser,
  falling_dispenser,
  decoyed,
  accelerating_alias,
};
struct stage {
  uint16_t ticks;
  flight_stage motion;
};
struct definition {
  std::string label{};
  std::string carried{};
  std::string fired{};
  std::string secondary{};
  weapon_role role{weapon_role::air};
  uint8_t category{0};
  bool impact_effects{true};                                                   // Original byte 30; independent of guidance category at byte 29.
  int16_t mass{0};
  std::array<uint16_t, 3> mount_masks{};                                       // Eurofighter, F-117, F-22; one bit per store slot.
  int32_t range{0};
  int16_t minimum_range{0};
  int16_t launch_speed{0};
  bool player_proximity{false};                                                // Original byte 43 enables nearby-player warnings and trail emission.
  float drag{0};
  std::vector<stage> stages{};
};
std::vector<definition> load_definitions(std::span<uint8_t const> executable);

} // namespace tfx::weapons
