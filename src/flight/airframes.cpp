#include "airframes.h"
#include "aerodynamic_airframes.h"
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

namespace tfx::flight {
namespace {

unsigned int constexpr data_object_file{973476};

class preset_literals {
private:
  resources::binary_view const &source;
  unsigned int begin;
  unsigned int end;

public:
  preset_literals(resources::binary_view const &source, unsigned int const begin, unsigned int const end) : source{source}, begin{begin}, end{end} {
    source.slice(begin,end - begin);
  }
  double number(unsigned int const field) const {
    // Read the two literal words assigned to a compiled double. This recognises
    // only MOV [absolute address], immediate data; no original code executes.
    std::array<std::optional<uint32_t>,2> words;
    for(unsigned int at{begin}; at + 10 <= end; ++at) {
      if(source.le16(at) != 0x05c7) continue;
      auto const address{source.le32(at + 2)};
      if(address != field && address != field + 4) continue;
      auto &word{words[(address - field) / 4]};
      auto const value{source.le32(at + 6)};
      if(word && *word != value) throw std::runtime_error{"ambiguous aircraft preset literal"};
      word = value;
    }
    if(!words[0] || !words[1]) throw std::runtime_error{"missing aircraft preset literal at data offset " + std::to_string(field)};
    return std::bit_cast<double>(static_cast<uint64_t>(*words[0]) | (static_cast<uint64_t>(*words[1]) << 32));
  }
};

double constant(resources::binary_view const &source, unsigned int const field) {
  return std::bit_cast<double>(static_cast<uint64_t>(source.le32(data_object_file + field)) | (static_cast<uint64_t>(source.le32(data_object_file + field + 4)) << 32));
}
double root_argument(resources::binary_view const &source, unsigned int const low, unsigned int const high) {
  if(source.byte(low) != 0x68 || source.byte(high) != 0x68) throw std::runtime_error{"missing aircraft moment argument"};
  auto const argument{std::bit_cast<double>(static_cast<uint64_t>(source.le32(low + 1)) | (static_cast<uint64_t>(source.le32(high + 1)) << 32))};
  return std::sqrt(static_cast<double>(argument));
}

} // anonymous namespace

std::array<airframe,3> load_airframes(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  std::array<airframe,3> result;
  std::array<unsigned int,3> constexpr starts{0x98234,0x97598,0x96918}, ends{0x98eba,0x98231,0x97598};
  std::array<unsigned int,3> constexpr coordinates{0x13bd0,0x13c20,0x13c70};
  for(unsigned int aircraft{0}; aircraft != result.size(); ++aircraft) {
    preset_literals const literals{source,starts[aircraft],ends[aircraft]};
    auto &profile{result[aircraft]};
    profile.stores.aircraft = static_cast<player_aircraft>(aircraft + 1);
    profile.stores.fuel_capacity = literals.number(0x14850);
    for(unsigned int slot{0}; slot != store_slots; ++slot) for(unsigned int axis{0}; axis != coordinates.size(); ++axis) profile.mounts[slot][axis] = literals.number(coordinates[axis] + slot * 8u);
    if(aircraft == 0) {
      // Native 0x953a4 is sqrt, followed by these ordered extended-precision multiplications.
      profile.stores.moment_a = static_cast<double>(root_argument(source,0x985bc,0x985a3) * constant(source,0x28f8) * constant(source,0x2900) * constant(source,0x2908));
      profile.stores.moment_b = static_cast<double>(root_argument(source,0x985f7,0x985f2) * constant(source,0x2910));
    } else if(aircraft == 1) {
      profile.stores.moment_a = static_cast<double>(root_argument(source,0x9798c,0x97973) * constant(source,0x2848));
      profile.stores.moment_b = static_cast<double>(root_argument(source,0x979bb,0x979b6) * constant(source,0x2850));
    } else {
      profile.stores.moment_a = literals.number(0x142c8);
      profile.stores.moment_b = literals.number(0x14258);
    }
  }
  return result;
}

namespace {

struct angular_fields {
  unsigned int critical_degrees{0};
  unsigned int inverse_degrees{0};
  unsigned int aileron_degrees{0};
  unsigned int elevator_degrees{0};
  unsigned int flap_degrees{0};
  unsigned int automatic_flap_degrees{0};
};
struct gear_drag_fields {
  unsigned int main_multiplier;
  unsigned int main_radius;
  unsigned int scale;
  unsigned int nose_multiplier;
  unsigned int nose_radius;
  unsigned int main_strut;
  unsigned int nose_strut;
  unsigned int drag;
  unsigned int area_normalisation;
};
double gear_drag(resources::binary_view const &source, gear_drag_fields const &fields) {
  using calculation = double;
  auto const value{[&](unsigned int const field) { return constant(source,field); }};
  calculation const pi{value(0x579c)};
  calculation const main{pi * value(fields.main_multiplier) * value(fields.main_radius) * value(fields.scale) * value(fields.main_radius) * value(fields.scale)};
  calculation const nose{pi * value(fields.nose_multiplier) * value(fields.nose_radius) * value(fields.scale) * value(fields.nose_radius) * value(fields.scale)};
  // Keep the authored normalization literal, including the F-22's 50m2 value.
  return static_cast<double>((main + nose + value(fields.main_strut) + value(fields.nose_strut)) * value(fields.drag) * value(fields.area_normalisation));
}

} // anonymous namespace

std::array<aerodynamic_airframe,3> load_aerodynamic_airframes(std::span<uint8_t const> const executable) {
  resources::binary_view const source{executable};
  auto const stores{load_airframes(executable)};
  std::array<aerodynamic_airframe,3> result;
  std::array<unsigned int,3> constexpr starts{0x98234,0x97598,0x96918}, ends{0x98eba,0x98234,0x97598};
  std::array<angular_fields,3> constexpr angular{{
    {
      .critical_degrees{0x28a0},
      .inverse_degrees{0x28a8},
      .aileron_degrees{0x28c8},
      .elevator_degrees{0x28d0},
      .flap_degrees{0x28e0},
      .automatic_flap_degrees{0x28e8},
    },
    {
      .critical_degrees{0x27e8},
      .inverse_degrees{0x27f0},
      .aileron_degrees{0x2830},
      .elevator_degrees{0x2838},
    },
    {
      .critical_degrees{0x2768},
      .inverse_degrees{0x2770},
      .aileron_degrees{0x2778},
      .elevator_degrees{0x2780},
      .flap_degrees{0x2788},
      .automatic_flap_degrees{0x2790},
    },
  }};
  std::array<gear_drag_fields,3> constexpr gear{{
    {
      .main_multiplier{0x2900},
      .main_radius{0x2940},
      .scale{0x2948},
      .nose_multiplier{0x2950},
      .nose_radius{0x2958},
      .main_strut{0x2960},
      .nose_strut{0x2968},
      .drag{0x2970},
      .area_normalisation{0x2978},
    },
    {
      .main_multiplier{0x27f8},
      .main_radius{0x2870},
      .scale{0x2878},
      .nose_multiplier{0x2808},
      .nose_radius{0x2880},
      .main_strut{0x2888},
      .nose_strut{0x2890},
      .drag{0x2820},
      .area_normalisation{0x2898},
    },
    {
      .main_multiplier{0x27a0},
      .main_radius{0x27a8},
      .scale{0x27b0},
      .nose_multiplier{0x27b8},
      .nose_radius{0x27c0},
      .main_strut{0x27c8},
      .nose_strut{0x27d0},
      .drag{0x27d8},
      .area_normalisation{0x27e0},
    },
  }};
  for(unsigned int aircraft{0}; aircraft != result.size(); ++aircraft) {
    auto &profile{result[aircraft]};
    profile.stores_and_mounts = stores[aircraft];
    preset_literals const literals{source,starts[aircraft],ends[aircraft]};
    auto const &angles{angular[aircraft]};
    auto const value{[&](unsigned int const field) { return constant(source,field); }};
    auto const angle{[&](unsigned int const degrees) { return static_cast<double>(static_cast<double>(value(0x579c)) * value(degrees) * value(angles.inverse_degrees)); }};
    auto &p{profile.surfaces};
    p.wing_area = literals.number(0x147a0);
    p.wing_span = literals.number(0x14218);
    p.mean_chord = literals.number(0x14200);
    p.flap_lift_gradient = literals.number(0x141e0);
    p.maximum_flap_angle = aircraft == 1 ? literals.number(0x14088) : angle(angles.flap_degrees);
    p.flap_limit_gradient = literals.number(0x14148);
    p.maximum_lift = literals.number(0x14228);
    p.automatic_flap_gradient = literals.number(0x14140);
    p.maximum_automatic_flap_angle = aircraft == 1 ? literals.number(0x14080) : angle(angles.automatic_flap_degrees);
    p.zero_lift = literals.number(0x14248);
    p.critical_angle = angle(angles.critical_degrees);
    p.induced_drag = aircraft == 0 ? static_cast<double>(1.0 / (static_cast<double>(value(0x579c)) * value(0x28b0))) : literals.number(0x14150);
    p.air_brake_drag = literals.number(0x14430);
    p.gear_drag = gear_drag(source,gear[aircraft]);
    p.elevator_drag = aircraft == 0 ? static_cast<double>(static_cast<double>(p.induced_drag) * value(0x28b8) * value(0x28c0)) : literals.number(0x141b0);
    if(aircraft != 2) {
      auto const radius{value(aircraft == 0 ? 0x28d8 : 0x2868)};
      p.chute_area = static_cast<double>(static_cast<double>(value(0x579c)) * radius * radius);
      p.chute_pitch_arm = literals.number(0x140e0);
    }
    p.zero_pitch_moment = literals.number(0x141b8);
    if(aircraft == 0) p.air_brake_moment_arm = vec2d{static_cast<double>(value(0x2938) - static_cast<double>(literals.number(0x13b68))),literals.number(0x14108)};
    p.maximum_aileron_angle = angle(angles.aileron_degrees);
    p.maximum_elevator_angle = angle(angles.elevator_degrees);
    profile.coefficient_base = {
      .base_drag{literals.number(0x146a0)},
      .critical_mach{literals.number(0x14630)},
      .full_wave_drag_mach{literals.number(0x14640)},
      .wave_drag_increment{literals.number(0x14608)},
      .wing_area{p.wing_area},
      .additional_lifting_area{literals.number(0x8d00)},
      .pitch_stability_base{literals.number(0x141a8)},
      .pitch_damping_base{literals.number(0x14198)},
      .elevator_moment_gradient{literals.number(0x14178)},
      .elevator_lift_gradient{literals.number(0x14180)},
      .roll_damping_gradient{literals.number(0x14168)},
      .aileron_gradient{literals.number(0x140b0)},
    };
    profile.starting_coefficients = {
      .zero_lift_drag{aircraft == 0 ? literals.number(0x14160) : 0},
      .lift_slope{literals.number(0x14420)},
      .elevator_lift{aircraft == 1 ? literals.number(0x141d0) : 0},
    };
    auto const craft{static_cast<player_aircraft>(aircraft + 1)};
    profile.powerplant = {
      .aircraft{craft},
      .dry_thrust{literals.number(0x140a8)},
      .afterburner_thrust{literals.number(0x14078)},
      .afterburner_stages{source.byte(data_object_file + 0x8cd8 + aircraft)},
      .ceiling_km{literals.number(0x14128)},
      .dry_fuel_coefficient{literals.number(0x140c0)},
      .afterburner_fuel_coefficient{literals.number(0x14070)},
      .roll_thrust_arm{literals.number(0x14120)},
      .pitch_thrust_arm{literals.number(0x14118)},
    };
    profile.viewpoint = {
      .forward{literals.number(0x8d28)},
      .upward{literals.number(0x8d30)},
    };
    preset_literals const common{source,0x95f3c,0x96918};
    auto &ground{profile.ground};
    std::array<unsigned int,6> constexpr contact_offsets{0x13ed0,0x145d8,0x13df8,0x13d60,0x13d78,0x13e10};
    // F-22 overwrites its preliminary wing/body points later in the same
    // straight-line preset. EF gear-forward positions subtract its authored CG.
    preset_literals const final_body{source,aircraft == 2 ? 0x96ee8 : starts[aircraft],ends[aircraft]};
    for(unsigned int point{0}; point != ground.offsets.size(); ++point) for(unsigned int axis{0}; axis != 3; ++axis) {
      if(aircraft == 0 && point < 3 && axis == 0) ground.offsets[point][axis] = static_cast<double>(static_cast<double>(value(point == 0 ? 0x2928 : 0x2930)) - literals.number(0x13b68));
      else ground.offsets[point][axis] = (point < 3 ? literals : final_body).number(contact_offsets[point] + axis * 8u);
    }
    for(unsigned int axis{0}; axis != 3; ++axis) ground.hook_offset[axis] = literals.number(0x13dd8 + axis * 8u);
    ground.maximum_gear_speed = literals.number(0x14550);
    ground.arrestor_stiffness = literals.number(0x13d40);
    ground.arrestor_damping = literals.number(0x13d98);
    ground.contact = {
      .spring_stiffness{literals.number(0x14570)},
      .damping{literals.number(0x14278)},
      .maximum_sink_speed{literals.number(0x14568)},
      .rolling_warning_speed{literals.number(0x14560)},
      .rolling_failure_speed{literals.number(0x14558)},
      .brake_warning_speed{literals.number(0x13d38)},
      .brake_failure_speed{literals.number(0x13d30)},
      .braking_coefficient{literals.number(0x13e50)},
      .wet_rolling_coefficient{common.number(0x13e88)},
      .wet_braking_factor{common.number(0x13e80)},
      .empty_mass_kg{literals.number(0x14220)},
      .left_main_wheel_right{ground.offsets[1].y},
      .unloaded_clearance{literals.number(0x13da0)},
    };
    profile.empty_mass_kg = literals.number(0x14220);
    profile.maximum_g = literals.number(0x14488);
    profile.minimum_g = literals.number(0x14500);
    profile.cockpit_upward_metres = literals.number(0x8d20);
    profile.stall_metres_per_second = literals.number(0x14540);
    profile.low_speed_reference = literals.number(0x14548);
    profile.cruise_mach = literals.number(0x14698);
    preset_literals const carrier{source,0x968d4,0x96918};
    preset_literals const atmosphere{source,0x95d44,0x95dc8};
    profile.atmosphere = {
      .sea_level_density{atmosphere.number(0x141e8)},
      .temperature_lapse_per_metre{atmosphere.number(0x14210)},
      .temperature_lapse_limit_metres{atmosphere.number(0x14260)},
    };
    profile.rudder_effectiveness = literals.number(0x140c8);
    profile.maximum_autopilot_speed = literals.number(0x13d28);
    profile.carrier_setup = {
      .maximum_sink_speed{carrier.number(0x14568)},
      .wing_area{carrier.number(0x147a0)},
      .air_brake_drag{carrier.number(0x14430)},
    };
  }
  return result;
}

void aerodynamic_airframe::use_carrier_setup() {
  ground.contact.maximum_sink_speed = carrier_setup.maximum_sink_speed;
  surfaces.wing_area = coefficient_base.wing_area = carrier_setup.wing_area;
  surfaces.air_brake_drag = carrier_setup.air_brake_drag;
}

aerodynamic_coefficient_parameters aerodynamic_airframe::initial_coefficients(double const previous_flap_lift_offset) const {
  using calculation = double;
  auto result{coefficient_base};
  auto const &p{surfaces};
  double constexpr control_multiplier{2.8}, moment_scale{.00001}, damping_scale{-4};
  calculation const capacity{static_cast<calculation>(p.flap_limit_gradient) * p.maximum_flap_angle + p.maximum_lift + static_cast<calculation>(p.automatic_flap_gradient) * p.maximum_automatic_flap_angle - previous_flap_lift_offset};
  calculation const gradient{static_cast<calculation>(result.elevator_moment_gradient) * p.maximum_elevator_angle * control_multiplier / capacity};
  result.pitch_stability_correction = static_cast<double>(gradient - result.pitch_stability_base);
  calculation const combined{static_cast<calculation>(result.pitch_stability_base) + result.pitch_stability_correction};
  result.pitch_damping_mach_gradient = static_cast<double>(std::sqrt(-combined * stores_and_mounts.stores.moment_b * moment_scale) * damping_scale - result.pitch_damping_base);
  return result;
}

} // namespace tfx::flight
