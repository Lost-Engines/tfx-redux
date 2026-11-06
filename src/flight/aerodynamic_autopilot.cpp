#include "aerodynamic_autopilot.h"
#include <cmath>
#include "flight/aerodynamic_control_law_internal.h"

namespace tfx::flight {
namespace {
using calculation = double;
double constexpr gravity{9.81};
int32_t constexpr minimum_downward_axis{32}, maximum_right_axis{32760};
void append(aerodynamic_guidance_events &events, afterburner_change const change) {
  if(change != afterburner_change::none) events.afterburner.push_back(change);
}
} // anonymous namespace

aerodynamic_guidance_events update_aerodynamic_autopilot(aerodynamic_autopilot &state, aerodynamic_manual_state &manual, aerodynamic_powerplant &powerplant, aerodynamic_motion &motion, aerodynamic_autopilot_context const &context) {
  aerodynamic_guidance_events events;
  auto const &law{context.guidance.law};
  auto const &data{law.data};
  auto const &targets{manual.controls.autopilot};
  // This gate tests the vertical component of the local RIGHT axis: bank,
  // rather than pitch. The original checks the quantized physical frame.
  int32_t const right_down{math::signed_word(static_cast<uint16_t>(motion.pose.frame.right.z))};
  int32_t const down_down{math::signed_word(static_cast<uint16_t>(motion.pose.frame.down.z))};
  calculation const dynamic{static_cast<calculation>(data.atmosphere.density) * data.metres_per_second * data.metres_per_second * law.geometry.wing_area};
  double const stored_dynamic{static_cast<double>(dynamic)};
  double const weight{static_cast<double>(static_cast<calculation>(data.mass_kg) * 2 * gravity)};
  calculation const available_lift{dynamic * law.surfaces.positive_lift_limit};
  calculation const available_g_lift{static_cast<calculation>(stored_dynamic) * manual.positive_g_lift_limit};
  if(targets.enabled && down_down > minimum_downward_axis && std::abs(right_down) < maximum_right_axis && (!(available_lift >= weight) || !(available_g_lift >= weight))) {
    aerodynamic_speed_state speed{
      .target_metres_per_second{targets.metres_per_second},
      .air_brake_fraction{manual.air_brake_fraction},
      .air_brake_condition{manual.air_brake_condition},
    };
    append(events,update_aerodynamic_speed(speed,powerplant,{
      .metres_per_second{data.metres_per_second},
      .mass_kg{data.mass_kg},
      .forward_force{law.body_forward_force},
      .air_brake_drag{law.geometry.air_brake_drag},
      .afterburner_stages{context.guidance.engine.afterburner_stages},
      .strategy{aerodynamic_speed_strategy::recover},
    }));
    manual.air_brake_fraction = speed.air_brake_fraction;
    manual.air_brake_condition = speed.air_brake_condition;
  }
  if(!targets.enabled || right_down >= maximum_right_axis || right_down <= -maximum_right_axis || !(available_lift > weight) || !(available_g_lift > weight)) {
    append(events,update_aerodynamic_manual(manual,powerplant,law));
    return events;
  }
  update_aerodynamic_navigation(state.navigation,state.guidance,manual.controls.autopilot,motion,context.navigation);
  auto const guided{update_aerodynamic_guidance(state.guidance,manual,powerplant,motion,state.recovery,context.guidance)};
  events.afterburner.insert(events.afterburner.end(),guided.afterburner.begin(),guided.afterburner.end());
  events.recovery = guided.recovery;
  apply_aerodynamic_guidance(state.guidance,manual,powerplant,context.guidance);
  append(events,finish_aerodynamic_controls(manual,powerplant,law));
  return events;
}

} // namespace tfx::flight
