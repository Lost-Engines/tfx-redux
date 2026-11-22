#include "progress.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "weapons/definition.h"

namespace tfx::missions {
namespace {

int32_t add(int32_t const first, int32_t const second) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(first) + static_cast<uint32_t>(second));
}
int16_t word(int const value) {
  return math::signed_word(static_cast<uint16_t>(value));
}

} // anonymous namespace

progress::progress(setup const &source) : initialisation{source}, reported(source.targets.size()), remaining_timer{source.timer.value_or(-1)} {
  reset();
}

void progress::reset() {
  results = {};
  results.targets.resize(initialisation.targets.size(),target_life::alive);
  std::ranges::fill(reported,0);
  return_score = 0; target_cursor = 0; violations = 0; exit_ticks = 0;
  remaining_timer = initialisation.timer.value_or(-1);
  beyond_airspace = false;
  resolved = false; ended = false; force_landed_return = false; critical_loss = false; player_ejected = false; impact_started = false; death_ticks = 0;
  outcome = session_outcome::normal; status = session_status::normal;
}

void progress::begin_contact_explosion() {
  // Native 0x7150c has already selected camera 27 and kapow before 0x7d270.
  player_ejected = true;
  impact_started = false;
  death_ticks = 200;
}

player_loss_effects progress::observe_player_loss(player_loss_context const &context) {
  // Native 0x7d270: loss/ejection and impact are separate stages; the post-impact exit waits strictly past zero.
  player_loss_effects effects;
  if(context.contact_explosion && (!context.invulnerable || player_ejected)) {
    effects.force_collision = true;
    outcome = session_outcome::aircraft_lost;
    status = session_status::aircraft_lost;
    death_ticks = word(death_ticks - context.elapsed);
    if(death_ticks < 0) ended = true;
    return effects;
  }
  if((!context.invulnerable || player_ejected) && (context.fatal || context.collision)) {
    effects.first_loss = !player_ejected && !context.pilot_active;
    if(effects.first_loss) player_ejected = true;
    outcome = session_outcome::aircraft_lost;
    status = session_status::aircraft_lost;
    if(effects.first_loss && !context.collision) {
      int32_t constexpr mayday_height{-200};
      effects.explosion = true;
      effects.mayday = context.height < mayday_height;
    }
  }
  if(context.collision && !impact_started && !context.pilot_active) {
    impact_started = true;
    effects.first_impact = true;
    death_ticks = 200;
    effects.explosion = true;
  }
  if(impact_started) {
    death_ticks = word(death_ticks - context.elapsed);
    if(death_ticks < 0) ended = true;
  }
  return effects;
}

loss_effects progress::record_air_loss(bool const enemy_pool, int16_t const type, bool const player_owned, bool const authorised, math::random_state &random) {
  loss_effects effects;
  bool const matches{initialisation.aircraft_type_filter == -1 || type == initialisation.aircraft_type_filter};
  if(enemy_pool) {
    // 0x782f8 acknowledges a player hit before fatal-aircraft handling consumes its ejection random value.
    if(player_owned) effects.acknowledgement_sample = random.next() & 1 ? 0x46u : 0x22u;
    if(authorised) results.personal_air_score = add(results.personal_air_score,1000);
    else { violations = word(violations + 1);effects.orders_violated = true; }
    results.credited_air_kills = add(results.credited_air_kills,1);
    if(matches) results.enemy_aircraft_down = add(results.enemy_aircraft_down,1);
  } else {
    if(matches) results.friendly_aircraft_down = add(results.friendly_aircraft_down,1);
    effects.friendly_target = true;
  }
  return effects;
}

loss_effects progress::record_surface_loss(bool const hostile, int16_t const type) {
  // 0x7c418 awards the mobile kill/score even for another actor's shot and for a friendly victim.
  results.credited_ground_kills = add(results.credited_ground_kills,1);
  results.personal_air_score = add(results.personal_air_score,1000);
  if(hostile && (initialisation.surface_type_filter == -1 || type == initialisation.surface_type_filter)) results.enemy_mobile_down = add(results.enemy_mobile_down,1);
  return {.friendly_target{!hostile}};
}

terrain_loss_effects progress::record_terrain_loss(terrain_loss_context const &context) {
  terrain_loss_effects effects;
  if(!context.player_owned) return effects;
  effects.explosion = true;
  // Native 0xa36e4 matches unwrapped cells, independently of radar/fire permission.
  effects.unauthorised = !std::ranges::any_of(initialisation.targets,[&](target const &value){
    return (value.position.x >> world::cell_shift) == context.cell.x && (value.position.z >> world::cell_shift) == context.cell.y;
  });
  // Native 0xa39c0 still requests the unauthorized-hit cue/sample for Arcade and decoys.
  if(!effects.unauthorised || context.arcade || context.weapon == std::to_underlying(weapons::weapon_id::chaff) || context.weapon == std::to_underlying(weapons::weapon_id::flare)) return effects;
  if(context.training) effects.notice = terrain_loss_notice::training_warning;
  else {
    violations = word(violations + 1);
    effects.notice = terrain_loss_notice::violation;
  }
  return effects;
}

void progress::set_termination(bool const arcade) {
  outcome = violations && !arcade ? session_outcome::violated_orders : session_outcome::normal;
  status = violations && !arcade ? session_status::violated_orders : session_status::normal;
  ended = true;
}

airspace_event progress::observe_airspace(airspace_context const &context) {
  // Native 0x71d74 divides toward zero, then compares the signed WORD cells.
  auto const cells{vec2<int16_t>{word(context.position.x / world::cell_units),word(context.position.y / world::cell_units)}};
  auto const [minimum,maximum]{std::minmax(cells.x,cells.y)};
  int16_t constexpr safe_first_cell{1}, safe_last_cell{world::map_width - 2};
  int16_t constexpr outer_first_cell{-2}, outer_last_cell{world::map_width + 1};
  int16_t constexpr departure_return_ticks{810};
  if(minimum >= safe_first_cell && maximum <= safe_last_cell) {
    beyond_airspace = false;
    return {};
  }
  airspace_event event{
    .cancel_acceleration{context.acceleration_active},
  };
  if(minimum < outer_first_cell || maximum > outer_last_cell) {
    if(!beyond_airspace) {
      beyond_airspace = true;
      violations = word(violations + 1);
      exit_ticks = departure_return_ticks;
      event.notice = airspace_notice::departed;
    }
  } else if(context.radio_idle || event.cancel_acceleration) event.notice = airspace_notice::approaching;
  return event;
}

std::vector<progress_event> progress::begin_frame(progress_context const &context) {
  if(context.targets.size() != reported.size()) throw std::runtime_error{"mission target observations differ from the authored targets"};
  results.targets.assign(context.targets.begin(),context.targets.end());
  std::vector<progress_event> notices;
  // Native 0x7188c records each authored target once, in script order, independently of the killer.
  for(unsigned int index{0}; index != reported.size(); ++index) {
    if(results.targets[index] != target_life::dead || reported[index]) continue;
    reported[index] = 1;
    results.targets_destroyed = add(results.targets_destroyed,1);
    results.credited_targets = word(results.credited_targets + 1);
    if(initialisation.targets[index].text) notices.emplace_back(progress_notice{
      .kind{notice_kind::target_destroyed},
      .target{index}
    });
    if(initialisation.targets[index].time_limit != -1) remaining_timer = initialisation.targets[index].time_limit;
    target_cursor = word(target_cursor + 1);
    if(target_cursor >= static_cast<int>(reported.size())) target_cursor = 0;
  }
  if(context.airspace) {
    auto boundary{*context.airspace};
    // Native target notices precede the boundary test and make the radio busy.
    if(!notices.empty()) boundary.radio_idle = false;
    auto const event{observe_airspace(boundary)};
    if(event.notice != airspace_notice::none || event.cancel_acceleration) notices.emplace_back(event);
  }
  if(!resolved && initialisation.completion) {
    auto const &rule{*initialisation.completion};
    bool completed{false};
    switch(rule.kind) {
    case completion_kind::enemy: completed = results.enemy_aircraft_down >= rule.value; break;
    case completion_kind::allied: completed = results.friendly_aircraft_down >= rule.value; break;
    case completion_kind::all_targets: completed = !results.targets.empty() && std::ranges::all_of(results.targets,[](auto const life){ return life == target_life::dead; }); break;
    case completion_kind::timer: case completion_kind::protect: completed = context.mission_time > static_cast<mission_tick>(rule.value); break;
    case completion_kind::enemy_mobile: completed = results.enemy_mobile_down >= rule.value; break;
    }
    if(completed) {
      resolved = true;
      results.completed = 1;
      notices.emplace_back(mission_completed_event{});
      if(rule.text && rule.kind != completion_kind::protect) notices.emplace_back(progress_notice{
        .kind{notice_kind::completion}
      });
      // Airborne starts return after ten native seconds; landed starts must return and stop.
      if(!initialisation.start.landed) exit_ticks = 540;
    } else if(rule.kind == completion_kind::protect && results.friendly_aircraft_down) {
      resolved = true;
      exit_ticks = 1620;
      if(rule.text) notices.emplace_back(progress_notice{
        .kind{notice_kind::protection_failed}
      });
    }
  }
  // 0x71a9c: landing after completion or an orders violation awards the authored return-to-base score.
  if(context.landed && !context.speed && (results.completed || violations)) {
    return_score = add(return_score,10000);
    set_termination(context.arcade);
  }
  return notices;
}

void progress::advance_return_countdown(simulation::timer_ticks const elapsed, bool const arcade) {
  // Native 71B90 runs after the aircraft and surface controllers, so their
  // final update still observes the player's flying speed and landed state.
  if(exit_ticks) {
    exit_ticks = word(exit_ticks - elapsed);
    if(exit_ticks <= 0) {
      exit_ticks = 0;
      force_landed_return = true;                                              // Native 71BBB/71BCA; this does not perform physical ground placement.
      if(critical_loss) { outcome = session_outcome::critical_loss; status = session_status::critical_loss; ended = true; } else set_termination(arcade);
    }
  }
}

bool progress::finish_frame(simulation::timer_ticks const elapsed) {
  // 0x70d74 runs only while the signed timer differs from -1, including after it becomes negative.
  if(remaining_timer == -1) return false;
  bool const expired{remaining_timer > 0 && add(remaining_timer,-static_cast<int32_t>(elapsed)) <= 0};
  remaining_timer = add(remaining_timer,-static_cast<int32_t>(elapsed));
  return expired;
}

} // namespace tfx::missions
