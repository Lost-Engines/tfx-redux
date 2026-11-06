#include "damage.h"
#include <utility>

namespace tfx::flight {

damage_state::damage_state() {
  systems.fill(system_condition::normal);
  systems[std::to_underlying(aircraft_system::landing_gear)] = system_condition::active;
}

system_condition damage_state::condition(aircraft_system const system) const {
  return systems.at(std::to_underlying(system));
}

void apply_damage(damage_state &state, int32_t const severity, math::random_state &random, bool const invulnerable, std::function<void()> const &notify) {
  // Native 0x8f460/0x8f3ec: every report samples eleven systems in this order, including repeated/zero reports.
  if(invulnerable) return;

  if(notify) notify();
  int16_t const percentage{math::signed_word(static_cast<uint16_t>(severity))};
  std::array<aircraft_system,11> constexpr sequence{
    aircraft_system::communications,aircraft_system::flaps,aircraft_system::hud,aircraft_system::wheel_brakes,
    aircraft_system::left_engine,aircraft_system::right_engine,aircraft_system::radar,aircraft_system::air_brake,
    aircraft_system::fuel,aircraft_system::weapons,aircraft_system::landing_gear,
  };
  double constexpr percentage_scale{100}, random_reciprocal{1.0 / 65535.0}, severity_scale{0.005};
  for(auto const system : sequence) {
    auto &condition{state.systems[std::to_underlying(system)]};
    double const threshold{static_cast<double>(static_cast<double>(random.next()) * percentage_scale * random_reciprocal)};
    if(percentage > threshold && condition != system_condition::failed) condition = system_condition::intermittent;
    if(percentage / 2 > threshold) condition = system_condition::failed;
  }
  state.accumulated = static_cast<double>(static_cast<double>(percentage) * severity_scale + state.accumulated);
  state.right_display_selection = 4;
}

void repair_systems(damage_state &state, simulation::timer_ticks const elapsed) {
  // 0x871d4: intermittent faults clear strictly after 1,080 ticks; failed systems never repair here.
  int16_t constexpr intermittent_repair_ticks{1080};
  for(unsigned int index{0}; index != state.systems.size(); ++index) {
    auto &ticks{state.repair_ticks[index]};
    if(state.systems[index] == system_condition::intermittent) {
      ticks = static_cast<uint16_t>(ticks + elapsed);
      if(math::signed_word(ticks) > intermittent_repair_ticks) { ticks = 0; state.systems[index] = system_condition::normal; }
    } else ticks = 0;
  }
}

} // namespace tfx::flight
