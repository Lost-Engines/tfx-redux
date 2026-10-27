#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "actors/air.h"
#include "actors/program.h"
#include "actors/surface.h"
#include "actors/surface_combat.h"
#include "actors/scenery.h"
#include "actors/contact_region.h"
#include "actors/visibility.h"
#include "audio/radar_voice.h"
#include "flight/arcade.h"
#include "flight/aerodynamic_launch.h"
#include "flight/aerodynamic_step.h"
#include "flight/aerodynamic_pilot_keys.h"
#include "flight/automatic_landing.h"
#include "flight/player_dispatch.h"
#include "flight/player_frame.h"
#include "flight/player_shared.h"
#include "flight/player_flight.h"
#include "flight/joystick.h"
#include "flight/mouse_input.h"
#include "pause_input.h"
#include "render/preview_state.h"
#include "flight/keyboard.h"
#include "flight/damage.h"
#include "flight/defence.h"
#include "missions/engagement.h"
#include "world/impact.h"
#include "world/effects.h"
#include "world/trails.h"
#include "flight/ejection_sequence.h"
#include "flight/simple.h"
#include "flight/fuel_gauge.h"
#include "cockpit/hud.h"
#include "cockpit/vector_hud.h"
#include "cockpit/radio_ticker.h"
#include "simulation/acceleration.h"
#include "cockpit/weapon_display.h"
#include "cockpit/external_display.h"
#include "cockpit/frame.h"
#include "cockpit/view_camera.h"
#include "cockpit/mouse_orbit.h"
#include "cockpit/initialisation.h"
#include "cockpit/scene_display.h"
#include "cockpit/designator.h"
#include "cockpit/designator_mouse.h"
#include "missions/initialisation.h"
#include "missions/timeline.h"
#include "missions/progress.h"
#include "missions/target_binding.h"
#include "missions/recovery.h"
#include "platform/window.h"
#include "platform/arming.h"
#include "platform/presentations.h"
#include "platform/flight.h"
#include "platform/configuration.h"
#include "platform/tactical_map.h"
#include "platform/screenshot.h"
#include "render/camera.h"
#include "render/point_projection.h"
#include "render/palette_selection.h"
#include "render/palette_lighting.h"
#include "render/glare.h"
#include "render/g_effects.h"
#include "flight/gear_controls.h"
#include "flight/engine_controls.h"
#include "flight/ground_service.h"
#include "render/framebuffer.h"
#include "render/display_pages.h"
#include "render/software_renderer.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"
#include "simulation/timing.h"
#include "sensors/air.h"
#include "sensors/ground.h"
#include "world/assets.h"
#include "world/environment.h"
#include "world/scene.h"
#include "world/terrain_cells.h"
#include "weapons/runtime.h"
#include "weapons/cannon.h"
#include "weapons/cannon_prediction.h"
#include "weapons/impact_prediction.h"
#include "recording/video.h"
#include "platform/recording.h"

namespace tfx::platform {
namespace {

class inventory_handoff {
private:
  weapons::pylon_inventory *destination;
  weapons::armament const &source;
public:
  inventory_handoff(weapons::pylon_inventory *const destination, weapons::armament const &source) : destination{destination}, source{source} {}
  ~inventory_handoff() { if(destination) *destination = source.pylons; }
  inventory_handoff(inventory_handoff const&) = delete;
  auto operator=(inventory_handoff const&)->inventory_handoff& = delete;
};

// Original INIT reads the live position/Mach and last WEAPONS index when a
// declaration omits them. Preserve the final values on every flight return,
// after camera/refuelling/result callbacks, while the source still exists.
class mission_handoff {
private:
  missions::inherited_setup *destination;
  flight::arcade_state const &source;
  uint8_t weapons;

public:
  mission_handoff(missions::inherited_setup *const destination, flight::arcade_state const &source, uint8_t const weapons)
    : destination{destination}, source{source}, weapons{weapons} {}
  ~mission_handoff() {
    if(!destination) return;
    destination->position = vec3<int32_t>{source.integer_position};
    destination->speed_factor = source.speed_mach;
    destination->weapons = weapons;
  }
  mission_handoff(mission_handoff const&) = delete;
  auto operator=(mission_handoff const&)->mission_handoff& = delete;
};

} // namespace

flight_result fly_mission(resources::archive const &game, resources::executable_tables const &tables,
  flight_options const &options, missions::variables &campaign, window *const shared_window) {
  auto video{options.video};
  if(shared_window && video.automatic_resolution) video.resolution = vec2<int>{shared_window->resolution()};
  auto &preferences{options.preferences};
  auto *const sound{options.sound};
  auto *active_window{shared_window};
  bool service_modal_closed{false};
  auto const sample{[&](audio::sample_priority const priority, auto const id) { if(sound) sound->sample(priority,id); }};
  auto const lapc_effect{[&](audio::lapc_effect_cue const cue) { if(sound) sound->lapc_effect(cue); }};
  auto const reheat_sound{[&](flight::afterburner_change const change) {
    if(change != flight::afterburner_change::none) sample(audio::sample_priority::normal,
      change == flight::afterburner_change::started || change == flight::afterburner_change::increased ? audio::sample_id::afterburner : audio::sample_id::afterburner_stop);
  }};
  bool simple_mode{preferences.model == simulation::flight_model::simple};
  bool arcade_mode{preferences.model == simulation::flight_model::arcade};
  auto const &mission_name{options.mission};
  auto const &override_environment{options.environment};
  auto const &override_grid{options.grid};
  auto const &override_cell{options.cell};
  auto const &screenshot{options.screenshot};
  unsigned int const scene_radius{options.inspect && options.scene_radius != 6 ? options.scene_radius : video.draw_distance};
  auto const scale{options.scale}, frame_limit{options.frame_limit}, flight_steps{options.flight_steps}, flight_frames{options.flight_frames};
  bool inspect{options.inspect};
  tfx::world::mission const selected{game,mission_name};
  auto const mission_setup{tfx::missions::initialise(selected.script,options.mission_state)};
  // Native mission entry 0x71138 replaces the executable image's prelaunch
  // limits with the TFX world bounds. Camera movement and HUD trajectories
  // share those initialized globals with moving actors and live projectiles.
  vec2<int32_t> const movement_height_bounds{tfx::actors::minimum_world_y,tfx::actors::maximum_world_y};
  tfx::world::environment environment{game,override_grid.empty() ? selected.grid_name() : override_grid,override_environment.empty() ? mission_setup.environment : override_environment};
  environment.lighting.status = tfx::world::apply_detail_status(environment.lighting.status,preferences.detail);
  std::array<tfx::render::dac_palette,tfx::render::altitude_palette_count> altitude_palettes;
  for(unsigned int bank{0}; bank != altitude_palettes.size(); ++bank) {
    auto const bytes{game.read("colours/" + (bank ? environment.lighting.upper_palette_names[bank - 1] : environment.lighting.palette_name) + ".col")};
    if(bytes.size() != altitude_palettes[bank].size()) throw std::runtime_error{"invalid environment altitude palette"};
    std::ranges::copy(bytes,altitude_palettes[bank].begin());
  }
  tfx::render::palette_lighting palette_lighting{altitude_palettes,environment.lighting.palette_altitudes,tables.fixed_flight_palette};
  tfx::render::g_effects_state g_effects;
  auto colours{tfx::render::expand_dac_palette(palette_lighting.colours())};
  auto const infrared_bytes{game.read("colours/" + environment.lighting.infrared_palette_name + ".col")};
  auto const infrared_colours{tfx::render::decode_palette(infrared_bytes)};
  tfx::render::dac_palette infrared_dac;
  std::ranges::copy(infrared_bytes,infrared_dac.begin());
  auto const *frame_colours{&colours};
  auto frame_dac{palette_lighting.colours()};
  tfx::render::palette_selection palette_selection;
  bool night_vision{false};
  int16_t hud_green{tfx::math::signed_word(preferences.hud_green)};
  tfx::world::assets assets{game};
  std::unique_ptr<tfx::world::environment> upper_environment;
  if(environment.lighting.cloud_layer) upper_environment = std::make_unique<tfx::world::environment>(game,environment.lighting.upper_grid,environment.lighting_name);
  tfx::world::scene scene{environment,assets,upper_environment.get(),&options.scenery_clock};
  auto &sky_state{options.sky};
  auto &ground_phase{options.ground_phase};
  bool sky_animation{false};
  tfx::render::software_renderer renderer{tables}, world_renderer{tables};
  std::array<tfx::render::software_renderer,2> instrument_renderers{tfx::render::software_renderer{tables},tfx::render::software_renderer{tables}};
  std::array<std::optional<vec2<int>>,2> instrument_positions{};
  world_renderer.output.resize(video.resolution);
  tfx::cockpit::vector_hud hud_vectors{tables.instruments()};
  std::vector<uint8_t> hud_underlay;
  tfx::render::framebuffer display_frame;
  display_frame.resize(video.resolution);
  scene.set_visual_lod_scale(video.lod_scale);
  tfx::render::display_pages frame_pages;
  tfx::missions::timeline mission_time{mission_setup};
  tfx::missions::progress mission_progress{mission_setup};
  tfx::flight::ground_service ground_service;
  tfx::missions::finish_program const mission_finish{selected.script};
  auto const initial_campaign{campaign};
  bool const frontend_flight{shared_window != nullptr};
  // Presentation state selects game rules independently of window ownership
  // and the numerical flight model. Direct launches supply that state too.
  bool const arcade_rules{campaign.contains("arcade") && campaign.at("arcade") != 0};
  // Native release 0x8bd8c keeps stores in Arcade, except the simulator graph.
  bool const unlimited_stores{arcade_rules && (!campaign.contains("simulator") || campaign.at("simulator") == 0)};
  bool result_evaluated{false};
  for(auto const &target : mission_setup.targets) scene.prepare_terrain_cell(static_cast<unsigned int>(target.position.x / tfx::world::cell_units),static_cast<unsigned int>(target.position.z / tfx::world::cell_units));
  for(auto const &event : mission_setup.scenery_events) scene.prepare_terrain_cell(event.cell.x,event.cell.y);
  auto const profiles{tfx::actors::profiles(game.read("waybin/craftype.cfg"))};
  tfx::actors::program const actor_program{game.read("waybin/" + mission_setup.waypoints + ".way")};
  auto const initial_actors{tfx::actors::initialise(actor_program,profiles,tables.formations)};
  auto const manoeuvres{tfx::actors::load_manoeuvres(game)};
  tfx::actors::air_runtime air_simulation{actor_program,initial_actors,profiles,tables.formations,tables.aircraft_envelopes,upper_environment ? upper_environment->variant_random_state() : environment.variant_random_state(),manoeuvres};
  // Native startup initializes weather and rain before grid loading resets
  // the shared generator. Retain the live stream for the next mission's setup.
  struct retained_random {
    tfx::math::random_state &session;
    tfx::math::random_state const &live;
    bool active;
    ~retained_random() { if(active) session = live; }
  } const random_return{
    .session{options.session_random},
    .live{air_simulation.random_stream()},
    .active{!inspect || flight_steps != 0 || flight_frames != 0},
  };
  std::array<std::optional<tfx::world::scene_handle>,tfx::actors::aircraft_per_side * 2> air_visuals;
  struct surface_actor {
    tfx::actors::surface_state state;
    tfx::world::scene_handle visual;
    tfx::actors::profile const *profile;
  };
  auto const visible_surface_contact{[](surface_actor const &actor) {
    auto constexpr excluded{std::to_underlying(tfx::actors::surface_flag::destroyed) | std::to_underlying(tfx::actors::surface_flag::retired)};
    auto const &pose{actor.state.pose};
    return !(pose.flags & excluded) && (pose.side == tfx::actors::allegiance::enemy || pose.side == tfx::actors::allegiance::allied);
  }};
  std::vector<surface_actor> surface_actors;
  tfx::cockpit::radio_ticker radio{tables.instruments().radio_padding};
  for(auto const &actor : initial_actors) {
    auto const profile{std::ranges::find(profiles,actor.profile_id,&tfx::actors::profile::id)};
    auto const visual{scene.create_actor(profile->definition,vec3d{actor.position} / tfx::actors::position_unit,actor.angles,actor.kind==tfx::actors::record_kind::aircraft)};
    if(actor.kind == tfx::actors::record_kind::aircraft) air_visuals[tfx::actors::air_registry_index(actor)] = visual;
    else if(actor.kind == tfx::actors::record_kind::surface) surface_actors.push_back({
      .state{tfx::actors::create_surface(actor,actor_program)},
      .visual{visual},
      .profile{&*profile},
    });
  }
  std::vector<tfx::world::scene_handle> fixed_scenery;
  std::vector<vec3<int32_t>> fixed_positions_q8;
  for(auto const &placement : tfx::actors::initialise_scenery(actor_program,profiles)) {
    auto const handle{scene.create_actor(placement.definition,vec3d{placement.position} / tfx::actors::position_unit,{0,0,0})};
    if(placement.event) scene.actor_event(handle,placement.event);
    fixed_scenery.push_back(handle);
    fixed_positions_q8.emplace_back(placement.position);
  }
  // 7572C submits ascending slots in the other-air pool, then allied air,
  // then surface actors and the fixed-scenery list. Authored creation order
  // can interleave those pools and changes the queue's equal-depth ties.
  std::vector<tfx::world::scene_handle> actor_draw_order;
  for(auto const &visual : air_visuals) if(visual) actor_draw_order.push_back(*visual);
  for(auto const &actor : surface_actors) actor_draw_order.push_back(actor.visual);
  actor_draw_order.insert(actor_draw_order.end(),fixed_scenery.begin(),fixed_scenery.end());
  scene.set_actor_draw_order(actor_draw_order);
  std::vector<tfx::missions::surface_target_candidate> surface_targets;
  for(auto const &actor : surface_actors) surface_targets.push_back({
    .slot{actor.state.pose.slot},
    .position_q8{vec3<int32_t>{actor.state.pose.position}},
    .flags{static_cast<uint8_t>(actor.state.pose.flags)},
  });
  std::vector<tfx::missions::target_binding> mission_targets;
  for(auto const &target : mission_setup.targets) mission_targets.push_back(tfx::missions::bind_target(target.position,fixed_positions_q8,surface_targets));
  auto const target_intact{[&](tfx::missions::target_binding const &target) {
    return std::visit([&](auto const &reference) {
      using reference_type = std::decay_t<decltype(reference)>;
      if constexpr(std::is_same_v<reference_type,tfx::missions::fixed_target>) return !scene.actor_destroyed(fixed_scenery.at(reference.index));
      else if constexpr(std::is_same_v<reference_type,tfx::missions::surface_target>) {
        auto const found{std::ranges::find_if(surface_actors,[&](auto const &actor) { return actor.state.pose.slot == reference.slot; })};
        if(found == surface_actors.end()) throw std::runtime_error{"bound mission target has lost its surface slot"};
        return !scene.actor_destroyed(found->visual);
      } else return scene.terrain_intact(static_cast<unsigned int>(reference.cell.x),static_cast<unsigned int>(reference.cell.y));
    },target);
  }};
  tfx::weapons::armament armament{mission_setup.aircraft,tables.weapons,tables.arming};
  armament.pylons = options.launch_inventory;
  auto const &launch_airframe{tables.aerodynamic_airframes.at(std::to_underlying(mission_setup.aircraft) - 1)};
  auto const launch_stores{armament.flight_stores(launch_airframe.stores_and_mounts.mounts)};
  inventory_handoff const remember_inventory{inspect ? nullptr : &options.launch_inventory,armament};
  armament.fit_default(mission_setup.weapons);
  if(frontend_flight && !inspect && !mission_setup.default_weapons) {
    auto const result{arm_mission(*shared_window,game,tables,armament,mission_setup.aircraft,mission_setup.weapons)};
    if(result == arming_result::closed) return flight_result::quit;
    if(result == arming_result::cancelled) return flight_result::cancelled;
  }
  if(frontend_flight && !inspect && !show_loading_screen(*shared_window,game)) return flight_result::quit;
  if(!mission_setup.default_weapons) {
    // Original arming leaves the last weapon thumbnail's aperture installed.
    // Direct launches reproduce DEFAULT/DONE's renderer handoff too.
    auto const &last_thumbnail{tables.arming_art().rows.back()};
    options.projection_origin = (last_thumbnail.minimum + last_thumbnail.maximum) / int16_t{2};
  }
  armament.last_released = options.last_released_mount; // Native DATA 0x16e98 survives mission changes.
  auto const initial_stores{armament.pylons};
  tfx::sensors::air_contacts radar;
  auto &seeker{options.seeker};
  std::vector<tfx::sensors::air_contact> radar_actors;
  tfx::sensors::designator designator;
  tfx::cockpit::designator_mouse designator_mouse;
  bool designator_view{false},designator_tracking_frame{false};
  uint32_t designator_frame_gap{0}; // Native 0x8844 counts input frames since the last designator draw.
  auto &cannon_navigation{options.cannon_navigation};
  tfx::flight::refuelling refuelling{tables.refuel_parameters()};
  tfx::flight::landing_system landing;
  tfx::flight::player_flight player_flight;
  auto &ejection{player_flight.ejection};
  tfx::missions::recovery_maps const recovery{game};
  auto const ejected{[&] { return ejection.value().stage != tfx::flight::ejection_stage::cockpit; }};
  std::optional<tfx::weapons::projectile_id> camera_missile;
  std::optional<unsigned int> impact_aircraft;
  std::optional<tfx::weapons::target_reference> ground_designation;
  vec3<int32_t> designated_point{0,0,0};
  std::optional<tfx::sensors::mobile_designation> mobile_designation;
  std::vector<tfx::sensors::surface_contact> ground_contacts;
  int16_t radiation_cursor{0};
  tfx::sensors::designator_controls designator_input;
  bool designate_trigger{false};
  tfx::weapons::cannon_state cannon;
  tfx::flight::damage_state player_damage;
  auto const instrument_setup{tfx::cockpit::initialise_instruments(mission_setup.aircraft,mission_setup.start.landed)};
  tfx::flight::defensive_systems const initial_defences{
    .chaff{instrument_setup.chaff},
    .flares{instrument_setup.flares},
    .stealth{instrument_setup.stealth},
  };
  auto defences{initial_defences};
  tfx::cockpit::hud_selection display_selection{instrument_setup.displays};
  // Native 0x71280 overrides the earlier instrument initializer for the
  // later sky renderer used by TFX environments. Cockpit keys can still move it.
  auto const initial_cockpit_offset{environment.lighting.initial_cockpit_raised ? vec2<int16_t>{instrument_setup.offset.x,0} : vec2<int16_t>{instrument_setup.offset}};
  tfx::cockpit::cockpit_position cockpit_position{tables.instruments(),initial_cockpit_offset};
  tfx::cockpit::view_control views;
  tfx::cockpit::view_camera view_motion;
  tfx::cockpit::mouse_orbit external_orbit;
  uint16_t view_pan{0};
  tfx::flight::keyboard_state keyboard;
  unsigned int ground_target_page{0};
  vec2<int32_t> const cruise_destination_q8{[&]() -> vec2<int32_t> {
    if(mission_targets.empty()) return {0,0};
    // Native 70C40 binds once at launch to the first target's initialized SSD
    // custom X/Z offset, without rotating it. N only changes the preview/HUD.
    auto const offset{std::visit([&](auto const &reference) {
      using target_type = std::decay_t<decltype(reference)>;
      if constexpr(std::is_same_v<target_type,tfx::missions::fixed_target>) return scene.actor_target_offset(fixed_scenery.at(reference.index));
      else if constexpr(std::is_same_v<target_type,tfx::missions::surface_target>) {
        auto const actor{std::ranges::find_if(surface_actors,[&](auto const &entry) { return entry.state.pose.slot == reference.slot; })};
        if(actor == surface_actors.end()) throw std::runtime_error{"bound cruise target has lost its surface slot"};
        return scene.actor_target_offset(actor->visual);
      } else return scene.terrain_target_offset(reference.cell);
    },mission_targets.front())};
    auto const &position{mission_setup.targets.front().position};
    return {
      std::bit_cast<int32_t>((static_cast<uint32_t>(position.x) + static_cast<uint32_t>(offset.x)) * tfx::actors::position_unit),
      std::bit_cast<int32_t>((static_cast<uint32_t>(position.z) + static_cast<uint32_t>(offset.y)) * tfx::actors::position_unit),
    };
  }()};
  auto &fuel_display{player_flight.fuel};
  fuel_display = instrument_setup.fuel;
  auto const &player_disabled{mission_progress.player_ejected};
  // Native DATA:12B88 is the loss camera's published target, distinct from
  // live physics. The contact-explosion return retains its previous pose.
  vec3<int32_t> disabled_camera_position_q8{0,0,0};
  auto &simple_player{player_flight.basic};
  auto &physical_player{player_flight.physical};
  auto &automatic_landing{player_flight.automatic};
  auto &stick_pressure{player_flight.stick_pressure};
  auto &simple_body{player_flight.simple_body};
  simple_body = simple_mode;
  auto const &wind_parameters{tables.aerodynamic_wind[tfx::flight::aerodynamic_initial_weather]};
  auto const surface_celsius{[&]() -> std::optional<int16_t> {
    if(!preferences.effects.temperature) return {};
    return tables.aerodynamic_temperatures[tfx::flight::aerodynamic_initial_weather][tfx::flight::aerodynamic_initial_temperature_column];
  }};
  std::vector<vec2<uint8_t>> player_route;
  for(auto const &point : actor_program.player_route) player_route.emplace_back(static_cast<uint8_t>(point.x / tfx::world::cell_units),static_cast<uint8_t>(point.z / tfx::world::cell_units));
  auto &player{simple_player.flight};
  // The player callback exports the renderer basis. A later ground-service
  // reset replaces the physics basis without exporting this cache again.
  math::attitude player_render_basis;
  scene.set_query_reference(&player.integer_position);
  tfx::flight::simple_inputs simple_controls;
  tfx::flight::mouse_flight_input mouse_control;
  tfx::simulation::time_acceleration acceleration;
  simple_controls.source = preferences.control;
  auto const body_basis{[&]() -> tfx::math::attitude const& { return player_flight.body(); }};
  player.aircraft = mission_setup.aircraft;
  tfx::actors::air_context air_world{
    .model{preferences.model},
    .intact_ground_target{[&](vec2<int16_t> const &cell) {
      return cell.x >= 0 && cell.y >= 0 && scene.terrain_intact(static_cast<unsigned int>(cell.x),static_cast<unsigned int>(cell.y));
    }},
    .combat{true},
    .player_aircraft{mission_setup.aircraft},
  };
  std::array<std::array<tfx::world::scene_handle,2>,tfx::weapons::weapon_count> weapon_visuals;
  for(unsigned int index{0}; index != tables.weapons.size(); ++index) {
    auto const &definition{tables.weapons[index]};
    for(unsigned int appearance{0}; appearance != 2; ++appearance) {
      auto const handle{scene.create_actor(appearance ? definition.secondary : definition.fired,{0,0,0},{0,0,0})};
      weapon_visuals[index][appearance] = handle;
      scene.show_actor(handle,false);
    }
  }
  std::array<char const*,3> constexpr player_definitions{"efa","f_117a","f_22"};
  auto const aircraft_visual{scene.create_actor(player_definitions.at(std::to_underlying(player.aircraft) - 1),{0,0,0},{0,0,0},true)};
  auto const pilot_visual{scene.create_actor("man",{0,0,0},{0,0,0})};
  struct crash_models {
    tfx::world::scene_handle land;
    tfx::world::scene_handle water;
    tfx::world::scene_handle contact;
  };
  crash_models const crashes{
    .land{scene.create_actor("crash",{0,0,0},{0,0,0})},
    .water{scene.create_actor("plop",{0,0,0},{0,0,0})},
    .contact{scene.create_actor("kapow",{0,0,0},{0,0,0})},
  };
  for(auto const handle : {crashes.land,crashes.water,crashes.contact}) scene.show_actor(handle,false);
  tfx::world::effect_pool effects;
  std::array<std::array<tfx::world::scene_handle,tfx::world::effect_kind_count>,tfx::world::effect_capacity> effect_visuals;
  for(auto &slot : effect_visuals) for(unsigned int kind{0}; kind != slot.size(); ++kind) {
    slot[kind] = scene.create_actor(std::string{tfx::world::effect_definitions[kind]},{0,0,0},{0,0,0});
    scene.show_actor(slot[kind],false);
  }
  auto &flak_state{options.flak};
  flak_state.centre = {0,0};
  tfx::world::flak_pool flak;
  bool flak_flash{false};
  bool const night_flak{(environment.lighting.status & tfx::world::night_status) != 0};
  std::array<std::vector<tfx::world::scene_handle>,tfx::world::flak_capacity> flak_visuals;
  for(auto &slot : flak_visuals) {
    unsigned int const count{night_flak ? tfx::world::flak_night_models : 1};
    slot.reserve(count);
    for(unsigned int model{0}; model != count; ++model) {
      auto const definition{night_flak ? tfx::world::night_flak_definitions[model] : tfx::world::day_flak_definition};
      auto const visual{scene.create_actor(std::string{definition},{0,0,0},{0,0,0})};
      scene.show_actor(visual,false);
      slot.push_back(visual);
    }
  }
  tfx::world::trail_pool trails;
  std::map<tfx::weapons::projectile_id,tfx::world::trail_sampler> trail_samplers;
  std::array<tfx::world::scene_handle,tfx::world::trail_capacity> trail_visuals;
  for(auto &visual : trail_visuals) {
    visual = scene.create_actor(std::string{tfx::world::trail_definition},{0,0,0},{0,0,0});
    scene.show_actor(visual,false);
  }
  auto const spawn_effect{[&](vec3<int32_t> const &position, tfx::world::effect_kind const kind) {
    if(kind == tfx::world::effect_kind::cannon_hit) sample(audio::sample_priority::normal,audio::sample_id::cannon_hit);
    auto const slot{effects.spawn(position,kind)};
    scene.actor_event(effect_visuals[slot][std::to_underlying(kind)],0);
  }};
  scene.set_effect_sink([&](tfx::world::effect_record const &request) {
    spawn_effect(vec3<int32_t>{request.position},request.kind);
  });
  auto const queue_effects{[&](tfx::render::camera const &camera) {
    vec3<int32_t> const reference{
      static_cast<int32_t>(std::floor(camera.position.x)),
      static_cast<int32_t>(std::floor(camera.position.y)),
      static_cast<int32_t>(std::floor(camera.position.z)),
    };
    for(auto const slot : effects.active_slots()) {
      auto const &effect{effects.record(slot)};
      scene.queue_actor(effect_visuals[slot][std::to_underlying(effect.kind)],vec3d{tfx::world::effect_position(effect.position,reference)},{0,0,0});
    }
    if(tfx::world::flak_visible(flak_state,player.integer_position)) for(auto const slot : flak.active_slots()) {
      auto const &effect{flak.record(slot)};
      scene.queue_actor(flak_visuals[slot].at(effect.model),vec3d{tfx::world::effect_position(effect.position,reference)},{0,0,0});
    }
    for(auto const slot : trails.active_slots()) scene.queue_actor(trail_visuals[slot],tfx::world::trail_position(trails.record(slot).position_q8,reference),{0,0,0});
  }};
  auto const clean_trails{[&] {
    auto const destroyed{[&](tfx::world::trail_slot const slot) { return scene.actor_destroyed(trail_visuals[slot]); }};
    trails.clean(player.integer_position,destroyed);
  }};
  std::optional<tfx::render::point_projection> impact_projection;
  std::optional<tfx::render::camera> trail_camera;
  auto const impact_surface{[&](vec3<int32_t> const &position) {
    auto const point{impact_projection ? impact_projection->integer(position) : std::nullopt};
    return tfx::world::classify_impact(renderer.output,point,environment.lighting.water_world);
  }};
  air_world.impact_effect = [&](vec3<int32_t> const &position) -> unsigned int {
    bool const water{scene.terrain_type(position) == 1 || impact_surface(position) == tfx::world::impact_surface::water};
    return std::to_underlying(water ? tfx::world::effect_kind::splash : tfx::world::effect_kind::crash);
  };
  auto player_visual{aircraft_visual};
  scene.show_actor(aircraft_visual,false);
  scene.show_actor(pilot_visual,false);
  tfx::simulation::timer_ticks launch_elapsed{5};
  auto const refuel_events{[&](std::span<tfx::flight::refuel_event const> const events) {
    for(auto const &event : events) {
      if(event.kind == tfx::flight::refuel_event_kind::player_model) scene.actor_event(player_visual,event.argument);
      else if(event.kind == tfx::flight::refuel_event_kind::tanker_model) scene.actor_event(*air_visuals.at(tfx::actors::aircraft_per_side + *event.tanker),event.argument);
      else if(event.kind == tfx::flight::refuel_event_kind::urgent_sample) sample(audio::sample_priority::urgent,event.argument);
      else if(event.kind == tfx::flight::refuel_event_kind::normal_sample) sample(audio::sample_priority::normal,event.argument);
      else if(event.kind == tfx::flight::refuel_event_kind::connected_voice && sound) sound->sequence(audio::sample_priority::normal,{std::to_underlying(audio::sample_id::fill_up),std::to_underlying(audio::sample_id::refuel_response)});
    }
  }};
  auto const update_refuelling{[&] {
    tfx::flight::refuel_registry allies;
    if(refuelling.value().enabled) for(unsigned int slot{0}; slot != allies.size(); ++slot) {
      unsigned int const index{tfx::actors::aircraft_per_side + slot};
      auto const &actor{air_simulation.aircraft()[index]};
      if(!actor) continue;
      auto const &pose{actor->flight.pose};
      allies[slot].emplace(tfx::flight::refuel_aircraft{
        .position_q8{vec3<int32_t>{pose.position}},
        .angles{vec3<tfx::math::angle>{pose.angles}},
        .flags{pose.flags},
        .shape_type{scene.actor_type(*air_visuals[index])},
      });
    }
    tfx::flight::refuel_player contact{
      .aircraft{player.aircraft},
      .position{vec3<int32_t>{player.integer_position}},
      .position_q8{vec3<int32_t>{vec3<long double>{player.position} * tfx::actors::position_unit}},
      .angles{vec3<tfx::math::angle>{player.angles}},
      .speed{player.fixed_speed},
    };
    auto const result{refuelling.advance(contact,allies,launch_elapsed)};
    player.integer_position = vec3<int32_t>{contact.position};
    if(result.target_override) radar.selected = tfx::actors::aircraft_per_side + *result.target_override;
    refuel_events(result.events);
    if(result.moved_tanker) {
      unsigned int const index{tfx::actors::aircraft_per_side + *result.moved_tanker};
      auto const &tanker{*allies[*result.moved_tanker]};
      air_simulation.apply_refuelling_pose(index,tanker.position_q8,tanker.angles);
      scene.place_actor(*air_visuals[index],vec3d{tanker.position_q8} / tfx::actors::position_unit,tanker.angles);
    }
  }};
  auto const damage_player{[&](int const percentage) {
    // Native 0x8f460 keeps damage reports and their RNG consumption after seat release.
    tfx::flight::apply_damage(player_damage,percentage,air_simulation.random_stream(),options.cheat.value().enabled,[&] { scene.actor_event(player_visual,13); });
    display_selection.right = static_cast<uint16_t>(player_damage.right_display_selection);
  }};
  tfx::missions::engagement_state engagement;
  tfx::weapons::active_projectile const *launch_warning{nullptr};
  auto const permission_context{[&] {
    return tfx::missions::engagement_context{
      .aircraft{player.aircraft},
      .arcade{arcade_rules},
      .training{campaign.contains("training") && campaign.at("training") != 0},
      .inhibited{ejected()},
    };
  }};
  std::array<std::optional<tfx::weapons::projectile_id>,tfx::actors::player_air_target> incoming;
  std::array<std::optional<tfx::actors::air_threat>,tfx::actors::player_air_target> threats;
  auto const actor_position{[&](tfx::weapons::actor_identity const identity) {
    switch(identity.pool) {
    case tfx::weapons::actor_pool::player: return vec3<int32_t>{options.player_contact.position};
    case tfx::weapons::actor_pool::aircraft: return vec3<int32_t>{air_simulation.aircraft()[identity.index]->flight.pose.position};
    case tfx::weapons::actor_pool::surface: return vec3<int32_t>{surface_actors.at(identity.index).state.pose.position};
    }
    throw std::runtime_error{"invalid projectile actor identity"};
  }};
  auto const horizontal_separation{[](vec3<int32_t> const &a, vec3<int32_t> const &b) {
    auto const delta{[](int32_t const x, int32_t const y) { return std::bit_cast<int32_t>(static_cast<uint32_t>(x) - static_cast<uint32_t>(y)); }};
    return std::max(std::abs(static_cast<int64_t>(delta(a.x,b.x))),std::abs(static_cast<int64_t>(delta(a.z,b.z))));
  }};
  auto const fatal_aircraft{[&](unsigned int const index, tfx::actors::air_loss_cause const cause = tfx::actors::air_loss_cause::combat) {
    if(!impact_aircraft) impact_aircraft = index;
    if(radar.selected == index) { radar.selected.reset(); seeker.guidance.reset(); }
    for(auto const &request : air_simulation.fatal_damage(index,cause)) if(auto const event{std::get_if<tfx::actors::air_scenery_event>(&request.data)}) scene.actor_event(*air_visuals[index],event->entry);
  }};
  auto const projectile_world_position{[](vec3<int32_t> const &position) {
    return vec3<int32_t>{position.x >> tfx::actors::position_fraction_bits,position.y >> tfx::actors::position_fraction_bits,position.z >> tfx::actors::position_fraction_bits};
  }};
  unsigned int enemy_aircraft_lost{0}, allied_aircraft_lost{0};
  tfx::weapons::runtime projectiles{tables.weapons,{
    .mount_state{[&] { return tfx::weapons::launch_mount{.angles{vec3<tfx::math::angle>{player.angles}},.selected{static_cast<uint8_t>(armament.selected)}}; }},
    .launch_elapsed{[&] { return launch_elapsed; }},
    .actor_position{actor_position},
    .designation_position{[&] { return vec3<int32_t>{designated_point}; }},
    .motion_inputs{[&](tfx::weapons::active_projectile const &shot,uint16_t const ticks) {
      bool const mounted{shot.owner.pool == tfx::weapons::actor_pool::player && (shot.state.motion == tfx::weapons::flight_stage::attached || shot.state.motion == tfx::weapons::flight_stage::attached_dispenser)};
      vec3<int32_t> const anchor{mounted ? tfx::math::place_anchor(vec3<int32_t>{vec3<uint32_t>{player.integer_position} * static_cast<uint32_t>(tfx::actors::position_unit)},player.angles,tables.store_mounts[std::to_underlying(player.aircraft) - 1].at(shot.mount)) : vec3<int32_t>{0,0,0}};
      return tfx::weapons::motion_context{
        .elapsed{ticks},
        .target{0,0,0},
        .player_owned{shot.owner.pool == tfx::weapons::actor_pool::player},
        .launch_anchor{vec3<int32_t>{anchor}},
        .player_angles{vec3<tfx::math::angle>{player.angles}},
        .player_speed{player.fixed_speed},
        .target_is_player{shot.target.actor && shot.target.actor->pool == tfx::weapons::actor_pool::player},
        .player_ejected{ejected()},
        .player_decoys{defences.ecm},
        .player_distance{tfx::math::distance_estimate(player.integer_position,projectile_world_position(shot.state.position))},
      };
    }},
    .player_distance{[&](tfx::weapons::active_projectile const &shot) {
      return tfx::math::distance_estimate(player.integer_position,projectile_world_position(shot.state.position));
    }},
    .warning{[&](tfx::weapons::projectile_warning const &warning) {
      if(sound) {
        if(warning.music) sound->music_cue(audio::music_cue_id::incoming_projectile);
        sound->sample(audio::sample_priority::urgent,warning.sample);
      }
    }},
    .reset_visual{[&](unsigned int const weapon,bool const secondary) { scene.actor_event(weapon_visuals[weapon][secondary],0); }},
    .launched{[&](tfx::weapons::active_projectile const &shot) {
      if(shot.owner.pool == tfx::weapons::actor_pool::player &&
         shot.state.weapon != std::to_underlying(tfx::weapons::weapon_id::cannon) &&
         shot.state.weapon != std::to_underlying(tfx::weapons::weapon_id::chaff) &&
         shot.state.weapon != std::to_underlying(tfx::weapons::weapon_id::flare)) camera_missile = shot.id;
      if(!shot.target.actor || shot.target.actor->pool != tfx::weapons::actor_pool::aircraft) return;
      auto const index{shot.target.actor->index};
      auto const &actor{air_simulation.aircraft()[index].value()};
      int64_t const distance{horizontal_separation(shot.state.position,actor.flight.pose.position)};
      auto const range{std::bit_cast<int32_t>(static_cast<uint32_t>(actor.combat.evasion_range) * (5280u * 32u))};
      if(distance > range) return;
      if(incoming[index]) {
        auto const found{projectiles.projectiles().find(*incoming[index])};
        if(found != projectiles.projectiles().end() && horizontal_separation(found->second->state.position,actor.flight.pose.position) < distance) return;
      }
      incoming[index] = shot.id;
      threats[index] = tfx::actors::air_threat{
        .position{vec3<int32_t>{shot.state.position}},
        .weapon{shot.state.weapon},
      };
    }},
    .target_cleared{[&](tfx::weapons::active_projectile const &shot) {
      if(shot.target.actor && shot.target.actor->pool == tfx::weapons::actor_pool::aircraft) {
        auto &current{incoming[shot.target.actor->index]};
        if(current == shot.id) { current.reset(); threats[shot.target.actor->index].reset(); }
      }
    }},
    .trail{[&](tfx::weapons::active_projectile const &shot) {
      if(!preferences.detail) return;
      // Keep spacing even when this segment is off screen. Visibility uses
      // the actual Redux lens rather than the original fixed 320x200 frustum.
      auto const &camera{trail_camera};
      bool const visible{camera && camera->sphere_visible(
        (vec3d{shot.state.previous} + vec3d{shot.state.position}) / (2 * tfx::actors::position_unit),
        (vec3d{shot.state.position} - vec3d{shot.state.previous}).length() / (2 * tfx::actors::position_unit))};
      trail_samplers[shot.id].advance(shot.state.previous,shot.state.position,[&](vec3<int32_t> const &position_q8) {
        if(!visible) return;
        auto const slot{trails.spawn(position_q8)};
        scene.actor_event(trail_visuals[slot],0);
      });
    }},
    .ground_impact{[&](tfx::weapons::active_projectile const &shot, int16_t &appearance) {
      auto const position{projectile_world_position(shot.state.position)};
      auto const &weapon{tables.weapons[shot.state.weapon]};
      std::optional<tfx::world::effect_kind> sampled;
      if(weapon.impact_effects) sampled = tfx::world::select_impact_effect(impact_surface(position),shot.state.weapon == std::to_underlying(tfx::weapons::weapon_id::cannon));
      auto const result{tfx::world::select_projectile_impact(appearance,{
        .weapon{shot.state.weapon},
        .sample_surface{weapon.impact_effects},
        .player_owned{shot.owner.pool == tfx::weapons::actor_pool::player},
        .sampled_effect{sampled},
      })};
      if(result.visual) spawn_effect(position,*result.visual);
      if(result.explosion) sample(audio::sample_priority::normal,audio::sample_id::aircraft_explosion);
    }},
    .contacts{[&](tfx::weapons::active_projectile &shot,uint16_t const ticks) {
      auto const source{weapon_visuals[shot.state.weapon][shot.state.secondary]};
      auto const previous{projectile_world_position(shot.state.previous)}, current{projectile_world_position(shot.state.position)};
      auto const response{[&](uint16_t const damage) { scene.apply_actor_contact_damage(source,damage); }};
      uint16_t const damage{scene.actor_impact_damage(source)};
      bool const released{tfx::weapons::has_swept_contacts(shot.state.motion)};
      // Native player dispatch precedes the source-destroyed test and has no ejection gate.
      if(released && !options.cheat.value().enabled && shot.owner.pool != tfx::weapons::actor_pool::player) scene.collide_actor(player_visual,previous,current,damage,response,damage_player);
      // Native A384F tests this before actor contacts, then still performs
      // the terrain/effect tail when one of those contacts destroys the shot.
      bool const contact_source_alive{!scene.actor_destroyed(source)};
      if(contact_source_alive && released && shot.state.weapon != std::to_underlying(tfx::weapons::weapon_id::chaff) && shot.state.weapon != std::to_underlying(tfx::weapons::weapon_id::flare)) {
        int32_t const broad_phase{std::bit_cast<int32_t>(static_cast<uint32_t>(shot.state.speed) * ticks * 20u)};
        for(unsigned int index{0}; index != air_simulation.aircraft().size(); ++index) {
          auto const &actor{air_simulation.aircraft()[index]};
          if(!actor || (shot.owner.pool == tfx::weapons::actor_pool::aircraft && shot.owner.index == index)) continue;
          auto const &pose{actor->flight.pose};
          if(pose.flags & (std::to_underlying(tfx::actors::air_flag::retired) | std::to_underlying(tfx::actors::air_flag::crashed))) continue;
          if(horizontal_separation(shot.state.position,pose.position) > broad_phase) continue;
          auto const visual{*air_visuals[index]};
          if(scene.actor_destroyed(visual)) continue;
          scene.collide_actor(visual,previous,current,damage,response);
          if(scene.actor_destroyed(visual)) {
            auto const effects{mission_progress.record_air_loss(index < tfx::actors::aircraft_per_side,scene.actor_type(visual),shot.owner.pool == tfx::weapons::actor_pool::player,engagement.authorised(permission_context()),air_simulation.random_stream())};
            if(effects.acknowledgement_sample) {
              if(sound) {
                sound->music_cue(audio::music_cue_id::player_aircraft_kill);
                sound->sequence(audio::sample_priority::normal,{std::to_underlying(audio::sample_id::aircraft_explosion),*effects.acknowledgement_sample});
              }
              radar.selected.reset(); seeker.tracking.reset(); seeker.guidance.reset();
            }
            if(effects.orders_violated) radio.show(tables.orders_violation_message,true);
            fatal_aircraft(index);
            ++(pose.side == tfx::actors::allegiance::allied ? allied_aircraft_lost : enemy_aircraft_lost);
          }
        }
        auto constexpr surface_destroyed{std::to_underlying(tfx::actors::surface_flag::destroyed)}, surface_retired{std::to_underlying(tfx::actors::surface_flag::retired)};
        for(unsigned int index{0}; index != surface_actors.size(); ++index) {
          auto &actor{surface_actors[index]};
          if(actor.state.pose.flags & surface_retired) continue;
          scene.collide_actor(actor.visual,previous,current,damage,response);
          if(!(actor.state.pose.flags & surface_destroyed) && scene.actor_destroyed(actor.visual)) {
            mission_progress.record_surface_loss((actor.state.pose.flags & std::to_underlying(tfx::actors::surface_flag::hostile)) != 0,scene.actor_type(actor.visual));
            actor.state.actions = std::to_underlying(tfx::actors::surface_action::dead);
            actor.state.input_ticks = 0;
            actor.state.pose.flags |= surface_destroyed;
          }
        }
        for(auto const handle : fixed_scenery) scene.collide_actor(handle,previous,current,damage,response);
      }
      if(contact_source_alive) {
        bool const terrain_destroyed{scene.terrain_destroyed(current)};
        scene.collide_terrain(previous,current,damage,false,response);
        if(!terrain_destroyed && scene.terrain_destroyed(current)) {
          auto const rules{permission_context()};
          auto const effects{mission_progress.record_terrain_loss({
            .cell{current.x >> tfx::world::cell_shift,current.z >> tfx::world::cell_shift},
            .player_owned{shot.owner.pool == tfx::weapons::actor_pool::player},
            .weapon{shot.state.weapon},
            .arcade{rules.arcade},
            .training{rules.training},
          })};
          if(effects.unauthorised && sound) sound->music_cue(audio::music_cue_id::unauthorised_target);
          if(effects.explosion) {
            if(effects.unauthorised && sound) sound->sequence(audio::sample_priority::normal,{std::to_underlying(audio::sample_id::aircraft_explosion),std::to_underlying(audio::sample_id::unauthorised_target)});
            else sample(audio::sample_priority::normal,audio::sample_id::aircraft_explosion);
          }
          if(effects.notice == tfx::missions::terrain_loss_notice::training_warning) radio.show(tables.ground_training_warning);
          else if(effects.notice == tfx::missions::terrain_loss_notice::violation) radio.show(tables.ground_orders_violation,true);
          if(shot.state.weapon == std::to_underlying(tfx::weapons::weapon_id::cbu55)) spawn_effect({current.x,0,current.z},tfx::world::effect_kind::boom);
        }
        // Every retail TFX environment sets native environment bit 1, enabling
        // clatter after either actor or terrain contacts destroy the shot.
        if(scene.actor_destroyed(source)) spawn_effect(current,tfx::world::effect_kind::clatter);
      }
      if(scene.actor_destroyed(source)) {
        shot.spent = true;
        scene.actor_event(source,0);
      }
    }},
  }};
  auto const emit_store{[&](tfx::weapons::release const &shot) {
    // Native 0x8bd8c announces release, opens internal bays, then constructs
    // the projectile. Alt+J suppresses audio but retains the bay event.
    if(!shot.jettisoned) {
      auto const sequence{audio::store_release_sequence(tables.weapons.at(shot.weapon).category)};
      lapc_effect(sequence.back() == std::to_underlying(audio::sample_id::bomb_release) ? audio::lapc_effect_cue::bomb_release : audio::lapc_effect_cue::missile_release);
      if(sound) sound->sequence(audio::sample_priority::normal,sequence);
    }
    if(player.aircraft != tfx::flight::player_aircraft::eurofighter) scene.actor_event(player_visual,5);
    projectiles.launch(shot);
    options.last_released_mount = shot.mount;
  }};
  air_world.emitted = [&](tfx::actors::air_request const &request) {
    std::visit([&](auto const &data) {
      using data_type = std::remove_cvref_t<decltype(data)>;
      if constexpr(std::is_same_v<data_type,tfx::actors::air_scenery_event>) scene.actor_event(*air_visuals.at(request.actor),data.entry);
      else if constexpr(std::is_same_v<data_type,tfx::actors::air_weapon>) {
        tfx::weapons::target_reference target{
          .position{vec3<int32_t>{data.target}},
        };
        if(data.target_actor) target.actor = tfx::weapons::actor_identity{
          .pool{*data.target_actor == tfx::actors::player_air_target ? tfx::weapons::actor_pool::player : tfx::weapons::actor_pool::aircraft},
          .index{*data.target_actor == tfx::actors::player_air_target ? 0u : *data.target_actor},
        };
        projectiles.launch({
          .weapon{data.weapon},
          .position{vec3<int32_t>{data.position}},
          .angles{vec3<tfx::math::angle>{data.angles}},
          .speed{data.speed},
          .owner{
            .pool{tfx::weapons::actor_pool::aircraft},
            .index{request.actor},
          },
          .target{target},
        });
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_countermeasure>) {
        tfx::weapons::actor_identity const owner{
          .pool{tfx::weapons::actor_pool::aircraft},
          .index{request.actor},
        };
        projectiles.countermeasure(owner,actor_position(owner),data.kind == tfx::actors::air_countermeasure_kind::radar,data.strength,air_simulation.random_stream());
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_warning>) {
        auto const kind{data.kind == tfx::actors::air_warning_kind::tracking ? tfx::missions::engagement_warning::tracking : tfx::missions::engagement_warning::firing};
        for(auto const &event : engagement.observe(kind,data.target == tfx::actors::player_air_target,permission_context())) {
          if(auto const request{std::get_if<tfx::missions::engagement_sample>(&event)}) sample(request->urgent ? audio::sample_priority::urgent : audio::sample_priority::normal,request->sample);
          else radio.show(tables.fire_permission_message);
        }
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_impact>) {
        spawn_effect(data.position,static_cast<tfx::world::effect_kind>(data.kind));
      } else if constexpr(std::is_same_v<data_type,tfx::actors::air_radio>) {
        radio.show(data.text);
      }
    },request.data);
  };
  auto const advance_actors{[&](unsigned int const ticks){
    // Native 0x75784 visits each actor once per elapsed tick, preserving pool order.
    for(unsigned int tick{0}; tick != std::max(1u,ticks); ++tick) {
      for(unsigned int index{0}; index != incoming.size(); ++index) {
        threats[index].reset();
        if(!incoming[index]) continue;
        auto const found{projectiles.projectiles().find(*incoming[index])};
        if(found != projectiles.projectiles().end()) threats[index] = tfx::actors::air_threat{
          .position{vec3<int32_t>{found->second->state.position}},
          .weapon{found->second->state.weapon},
        };
      }
      air_world.threats = threats;
      air_simulation.advance(air_world);
      std::vector<tfx::actors::surface_air_target> surface_air_targets;
      for(unsigned int index{0}; index != air_simulation.aircraft().size(); ++index) if(auto const &actor{air_simulation.aircraft()[index]}) surface_air_targets.push_back({
        .actor{index},
        .position{vec3<int32_t>{actor->flight.pose.position}},
        .flags{actor->flight.pose.flags},
      });
      for(unsigned int index{0}; index != surface_actors.size(); ++index) {
        auto &actor{surface_actors[index]};
        // Requests execute inside the route callback, before this unit's movement, matching the native launcher boundary.
        tfx::actors::advance_surface(actor.state,*actor.profile,[&](tfx::actors::surface_request const &request) {
          if(request.kind == tfx::actors::surface_request_kind::radio) {
            radio.show(request.text);
          } else if(request.kind == tfx::actors::surface_request_kind::nearby_targets) {
            for(auto const &shot : tfx::actors::engage_surface_targets(actor.state,*actor.profile,{
              .owner{index},
              .player{vec3<int32_t>{air_world.player->position}},
              .aircraft{player.aircraft},
              .aircraft_targets{surface_air_targets},
            },air_simulation.random_stream())) projectiles.launch(shot);
          } else if(request.kind == tfx::actors::surface_request_kind::ground_target) {
            bool const intact{request.cell.x >= 0 && request.cell.y >= 0 && scene.terrain_intact(static_cast<unsigned int>(request.cell.x),static_cast<unsigned int>(request.cell.y))};
            if(auto const shot{tfx::actors::surface_ground_attack(actor.state,index,request,intact)}) projectiles.launch(*shot);
          }
        });
      }
    }
    for(unsigned int index{0}; index != air_simulation.aircraft().size(); ++index) {
      auto const &actor{air_simulation.aircraft()[index]};
      if(!actor) continue;
      auto const visual{*air_visuals[index]};
      auto const &pose{actor->flight.pose};
      if(impact_aircraft == index && ((pose.flags & std::to_underlying(tfx::actors::air_flag::retired)) || actor->actions == std::to_underlying(tfx::actors::air_action::retire_crashed))) impact_aircraft.reset();
      scene.place_actor(visual,vec3d{pose.position} / tfx::actors::position_unit,pose.angles);
      scene.show_actor(visual,(pose.flags & std::to_underlying(tfx::actors::air_flag::retired)) == 0 && actor->actions != std::to_underlying(tfx::actors::air_action::retire_crashed));
    }
    for(auto const &actor : surface_actors) {
      scene.place_actor(actor.visual,vec3d{actor.state.pose.position} / tfx::actors::position_unit,actor.state.pose.angles);
      scene.show_actor(actor.visual,(actor.state.pose.flags & std::to_underlying(tfx::actors::surface_flag::retired)) == 0);
    }
  }};
  auto const select_actor_draw_region{[&](vec3<int32_t> const &anchor) {
    vec3<int32_t> const reference{vec3<uint32_t>{anchor} * static_cast<uint32_t>(tfx::actors::position_unit)};
    for(unsigned int index{0}; index != air_simulation.aircraft().size(); ++index) {
      auto const &actor{air_simulation.aircraft()[index]};
      if(!actor) continue;
      scene.show_actor(*air_visuals[index],
        !(actor->flight.pose.flags & std::to_underlying(tfx::actors::air_flag::retired))
        && actor->actions != std::to_underlying(tfx::actors::air_action::retire_crashed)
        && (video.lod_scale == 0 || scene_radius == 0 || scene_radius > tfx::world::terrain_neighbourhood_width / 2u || tfx::actors::aircraft_in_draw_region(actor->flight.pose.position,reference)));
    }
    for(auto const &actor : surface_actors) scene.show_actor(actor.visual,
      !(actor.state.pose.flags & std::to_underlying(tfx::actors::surface_flag::retired))
      && (video.lod_scale == 0 || scene_radius == 0 || scene_radius > tfx::world::terrain_neighbourhood_width / 2u || tfx::actors::surface_in_draw_region(actor.state.pose.position,reference)));
  }};
  tfx::cockpit::hud instruments{tables.instruments(),game,tables.weapons,environment.lighting.schematic_name};
  tfx::cockpit::scene_displays scene_displays{tables.instruments()};
  tfx::cockpit::designator_display designator_display{tables.instruments(),game};
  tfx::cockpit::frame cockpit_frame{tables.instruments(),game};
  tfx::cockpit::weapon_display aiming{tables.instruments(),game};
  tfx::cockpit::tactical_map tactical_map{game,tables,environment.lighting.schematic_name,options.map_animation};
  tfx::cockpit::external_display const external_instruments{tables.instruments(),tables.presentation_fonts()};
  // The native ground-start fallback is zero for type 50, -20 otherwise (0x8c318).
  auto const start_x{static_cast<unsigned int>(std::clamp(mission_setup.start.position.x / tfx::world::cell_units,0,static_cast<int>(tfx::world::map_width) - 1))};
  auto const start_z{static_cast<unsigned int>(std::clamp(mission_setup.start.position.z / tfx::world::cell_units,0,static_cast<int>(tfx::world::map_height) - 1))};
  auto const flags{assets.object(environment.variant(start_x,start_z)).flags};
  tfx::flight::ground_contact ground{
    .present{true},
    .height{static_cast<int16_t>(mission_setup.start.landed && (!(flags & 2) || (flags & 255) >> 2 != 50) ? -20 : 0)},
  };
  auto const initial_ground{ground};
  int16_t ground_zone{-1};
  auto const view_index{[&] { return static_cast<unsigned int>(std::to_underlying(designator_view ? tfx::cockpit::view_mode::designator : views.value().current)); }};
  auto const camera_context{[&] {
    tfx::cockpit::view_camera_context context{
      .position{vec3<int32_t>{player.integer_position}},
      .position_q8{vec3<int32_t>{vec3<long double>{player.position} * tfx::actors::position_unit}},
      .position_f32{vec3f{player.position}},
      .angles{vec3<tfx::math::angle>{player.angles}},
      .basis{player_render_basis},
      .body_basis{&body_basis()},
      .elapsed{launch_elapsed},
      .pan{view_pan},
      .ground_height{ground.present ? ground.height : int16_t{0}},
      .refuel_phase{refuelling.value().phase},
      .speed{player.fixed_speed},
      .height_bounds{vec2<int32_t>{movement_height_bounds}},
    };
    if(ground_designation) context.tracking_target_q8.emplace(vec3<int32_t>{designated_point});
    else if(seeker.guidance && air_simulation.aircraft()[*seeker.guidance]) context.tracking_target_q8.emplace(vec3<int32_t>{air_simulation.aircraft()[*seeker.guidance]->flight.pose.position});
    else if(radar.selected && air_simulation.aircraft()[*radar.selected]) context.tracking_target_q8.emplace(vec3<int32_t>{air_simulation.aircraft()[*radar.selected]->flight.pose.position});
    if(player_disabled) context.impact_target_q8.emplace(vec3<int32_t>{disabled_camera_position_q8});
    else if(impact_aircraft && air_simulation.aircraft()[*impact_aircraft]) context.impact_target_q8.emplace(vec3<int32_t>{air_simulation.aircraft()[*impact_aircraft]->flight.pose.position});
    if(context.impact_target_q8) context.observed_target_q8.emplace(vec3<int32_t>{*context.impact_target_q8});
    else if(seeker.guidance && air_simulation.aircraft()[*seeker.guidance]) context.observed_target_q8.emplace(vec3<int32_t>{air_simulation.aircraft()[*seeker.guidance]->flight.pose.position});
    else if(context.tracking_target_q8) context.observed_target_q8.emplace(vec3<int32_t>{*context.tracking_target_q8});
    if(camera_missile) if(auto const shot{projectiles.projectiles().find(*camera_missile)}; shot != projectiles.projectiles().end() && !shot->second->spent) context.missile.emplace(tfx::cockpit::missile_camera_pose{
      .position_q8{vec3<int32_t>{shot->second->state.position}},
      .angles{vec3<tfx::math::angle>{shot->second->state.angles}},
    });
    return context;
  }};
  std::vector<vec3<int32_t>> map_targets;
  for(auto const &target : mission_setup.targets) map_targets.emplace_back(target.position);
  auto const apply_selection{[&](tfx::weapons::selection_change const &change) {
    if(change.clear_contact) radar.selected.reset();
    if(change.clear_tracking) { seeker.tracking.reset(); seeker.guidance.reset(); ground_designation.reset(); mobile_designation.reset(); }
  }};
  auto const reset_weapon_targets{[&] {
    radar.selected.reset();seeker.tracking.reset();seeker.guidance.reset();
    ground_designation.reset();mobile_designation.reset();
    armament.selected = 0;
    apply_selection(armament.select(player.aircraft == tfx::flight::player_aircraft::nighthawk ? tfx::weapons::weapon_role::ground : tfx::weapons::weapon_role::air,false));
  }};
  auto const finish_information_key{[&](bool const shift) {
    if(shift) {
      armament.fit_default(mission_setup.weapons);
      reset_weapon_targets();
      uint8_t constexpr restored_countermeasure_count{15};
      defences.chaff = defences.flares = restored_countermeasure_count;
    } else options.flight_information = !options.flight_information;
  }};
  auto const select_weapon{[&](tfx::weapons::weapon_role const role) {
    if(player.collision || player_disabled || ejected()) return;
    if(role == tfx::weapons::weapon_role::air) {
      physical_player.configure_controls(tfx::flight::input_configuration::combat);
      sample(audio::sample_priority::normal,audio::sample_id::control_selection);
      cannon_navigation = false;
    }
    if(armament.selected == tfx::weapons::no_pylon) armament.selected = 0;
    apply_selection(armament.select(role,true));
    // Native F-117 selection skips its first cannon result through the air selector.
    if(player.aircraft == tfx::flight::player_aircraft::nighthawk && armament.selected == tfx::weapons::cannon_pylon) apply_selection(armament.select(tfx::weapons::weapon_role::air,true));
    armament.guns = armament.selected == tfx::weapons::no_pylon || armament.selected == tfx::weapons::cannon_pylon;
    if(role == tfx::weapons::weapon_role::ground) {
      if(armament.guns) cannon_navigation = false;
      else physical_player.configure_controls(tfx::flight::input_configuration::cruise);
    }
  }};
  auto const selected_weapon{[&]()->tfx::weapons::definition const* {
    if(armament.guns || armament.selected < 0 || armament.selected >= tfx::weapons::cannon_pylon) return nullptr;
    auto const index{armament.pylons[static_cast<unsigned int>(armament.selected)].weapon};
    return index < tables.weapons.size() ? &tables.weapons[index] : nullptr;
  }};
  auto const refresh_ground_contacts{[&] {
    ground_contacts.clear();
    for(auto const &actor : surface_actors) ground_contacts.push_back({
      .slot{actor.state.pose.slot},
      .position{vec3<int32_t>{actor.state.pose.position}},
      .heading{actor.state.pose.angles.y},
      .side{actor.profile->side},
      .flags{static_cast<uint8_t>(actor.state.pose.flags)},
      .type{scene.actor_type(actor.visual)},
      .damage_centres{scene.actor_damage_centres(actor.visual)},
    });
  }};
  auto const surface_index{[&](unsigned int const slot) {
    auto const found{std::ranges::find_if(surface_actors,[&](auto const &actor) { return actor.state.pose.slot == slot; })};
    if(found == surface_actors.end()) throw std::runtime_error{"ground target refers to an absent surface slot"};
    return static_cast<unsigned int>(found - surface_actors.begin());
  }};
  auto const cycle_target{[&] {
    auto const *weapon{selected_weapon()};
    if(!weapon || weapon->role == tfx::weapons::weapon_role::air) { radar.next(); return; }
    if(weapon != &tables.weapons[std::to_underlying(tfx::weapons::weapon_id::harm)] && weapon != &tables.weapons[std::to_underlying(tfx::weapons::weapon_id::sidearm)]) return;
    refresh_ground_contacts();
    if(auto const slot{tfx::sensors::next_radiation_target(radiation_cursor,ground_contacts)}) {
      designated_point = vec3<int32_t>{surface_actors[surface_index(*slot)].state.pose.position};
      ground_designation.emplace(tfx::weapons::target_reference{
        .designated{true},
      });
      mobile_designation.reset();
    } else ground_designation.reset();
  }};
  auto const update_sensor_inputs{[&] {
    refresh_ground_contacts();
    if(mobile_designation) designated_point = tfx::sensors::mobile_target_point(surface_actors[surface_index(mobile_designation->slot)].state.pose.position,mobile_designation->offset);
    // Native 932C4 retries ARM acquisition while its target pointer is empty.
    if(!ground_designation && !armament.guns && !player.collision && !player_disabled) {
      auto const *weapon{selected_weapon()};
      if(weapon == &tables.weapons[std::to_underlying(tfx::weapons::weapon_id::harm)] || weapon == &tables.weapons[std::to_underlying(tfx::weapons::weapon_id::sidearm)]) cycle_target();
    }
  }};
  auto const update_designator{[&](bool frozen) {
    if(designator_view) {
      designator_tracking_frame = ground_designation.has_value();
      if(ground_designation) {
        designator_mouse.apply(designator,true);
        tfx::sensors::follow_designation(designator,player.integer_position,ground_designation->actor ? actor_position(*ground_designation->actor) : designated_point);
      } else {
        // Native 0x92c40 recentres only after more than one absent input frame.
        // Re-entering an already active view preserves its gimbal.
        if(!frozen && std::bit_cast<int32_t>(designator_frame_gap) > 1) {
          designator.pitch = 0x500;
          designator.heading = player.angles.y;
        }
        designator_mouse.apply(designator,false);
        tfx::sensors::aim_designator(designator,player.integer_position,player.angles,player.aircraft,frozen ? tfx::sensors::designator_controls{} : designator_input);
      }
      if(!frozen) designator_frame_gap = 0;
      if(!frozen && designate_trigger) {
        designated_point = vec3<int32_t>{vec3<uint32_t>{designator.point} * static_cast<uint32_t>(tfx::actors::position_unit)};
        auto const *weapon{selected_weapon()};
        if(weapon == &tables.weapons[std::to_underlying(tfx::weapons::weapon_id::maverick)]) {
          mobile_designation = tfx::sensors::nearest_mobile_target(designated_point,ground_contacts);
          if(mobile_designation) designated_point = tfx::sensors::mobile_target_point(surface_actors[surface_index(mobile_designation->slot)].state.pose.position,mobile_designation->offset);
        }
        ground_designation.emplace(tfx::weapons::target_reference{.designated{true}});
        designate_trigger = false;
      }
    }
  }};
  auto const advance_air_tracking{[&](tfx::render::point_projection const &projection) {
    radar_actors.clear();
    // The original enumerator visits the enemy pool backwards, then the allied pool backwards.
    for(unsigned int pool{0}; pool != 2; ++pool) for(unsigned int reverse{tfx::actors::aircraft_per_side}; reverse; --reverse) {
      unsigned int const index{pool * tfx::actors::aircraft_per_side + reverse - 1};
      auto const &actor{air_simulation.aircraft()[index]};
      if(!actor) continue;
      auto const &pose{actor->flight.pose};
      auto const profile{std::ranges::find(profiles,pose.profile_id,&tfx::actors::profile::id)};
      radar_actors.push_back({
        .actor{index},
        .position{vec3<int32_t>{pose.position}},
        .heading{pose.angles.y},
        .flags{pose.flags},
        .side{profile->side},
        .enemy_pool{pool == 0},
      });
    }
    unsigned int const weapon{armament.selected < 0 ? 0u : armament.pylons[static_cast<unsigned int>(armament.selected)].weapon};
    if(weapon >= tables.weapons.size()) { seeker.clear(); return; }
    tfx::sensors::advance_air_lock(seeker,radar,radar_actors,tables.weapons[weapon],{
      .position{vec3<int32_t>{player.integer_position}},
      .angles{vec3<tfx::math::angle>{player.angles}},
      .elapsed{launch_elapsed},
      .guns{armament.guns},
      .sensor_suppressed{defences.stealth},
      .centre{vec2<int16_t>{projection.origin}},
      .project{[&](vec3<int32_t> const &position) {
        if(auto const point{projection.integer(position)}) return vec2<int16_t>{*point};
        return vec2<int16_t>{-1,-1};
      }},
    });
    if(tables.weapons[weapon].category == tfx::weapons::cruise_missile_category) {
      auto const target{tfx::sensors::acquire_cruise_target(player.integer_position,cruise_destination_q8,[&](vec3<int32_t> const &position) {
        if(auto const point{projection.integer(position)}) return vec2<int16_t>{*point};
        return vec2<int16_t>{-1,-1};
      })};
      designated_point = vec3<int32_t>{target.position_q8};
      ground_designation.emplace(tfx::weapons::target_reference{
        .designated{true},
      });
      mobile_designation.reset();
      radar.selected.reset();
      seeker.tracking.reset();seeker.guidance.reset();
      seeker.projected = vec2<int16_t>{target.projected};
      seeker.reticle = vec2<int32_t>{target.reticle_q8};
    }
  }};
  tfx::simulation::timer_ticks constexpr first_animation_ticks{5}; // Native 0x7202f.
  tfx::simulation::timer_ticks previous_animation_ticks{first_animation_ticks};
  auto const restart{[&]{
    frame_pages.reset();
    previous_animation_ticks = first_animation_ticks;
    acceleration.cancel();
    ground = initial_ground;
    ground_zone = -1;
    simple_controls = {};
    simple_controls.source = preferences.control;
    mouse_control = {};
    mission_time.reset();
    mission_progress.reset();
    effects.reset();
    flak.reset();
    trails.reset();
    trail_samplers.clear();
    trail_camera.reset();
    flak_state.centre = {0,0};
    flak_flash = false;
    engagement = {};
    launch_warning = nullptr;
    result_evaluated = false;
    campaign = initial_campaign;
    for(auto const name : {"a2akills","a2gkills","num_miss","num_hours","miss_val","score","fmobile_down"}) campaign.try_emplace(name,0);
    player = {};
    cannon = {};
    player_damage = {};
    g_effects = {};
    display_selection = instrument_setup.displays;
    aiming.reset_tracking();
    player_damage.systems = instrument_setup.systems;
    ground_target_page = 0;
    fuel_display = instrument_setup.fuel;
    defences = initial_defences;
    armament.pylons = initial_stores;
    ground_service.reset();
    armament.selected = 0;
    bool const ground_weapons{mission_setup.aircraft == tfx::flight::player_aircraft::nighthawk};
    armament.select(ground_weapons ? tfx::weapons::weapon_role::ground : tfx::weapons::weapon_role::air,false);
    // Native command initialization at 0x83cb0 runs after automatic selection.
    // Even the cannon sentinel starts in the aircraft's weapon sight, not NAV.
    armament.guns = false;
    refuelling.reset();
    landing.reset();
    ejection.reset();
    automatic_landing = {};
    stick_pressure = true;
    simple_body = simple_mode;
    player_visual = aircraft_visual;
    // Native 0x7d720 installs the landed model stream before the loss/reset
    // pair at 0x7d7a3. Its gear state advances at the normal animation gate.
    if(mission_setup.start.landed) scene.actor_event(player_visual,25);
    scene.actor_event(player_visual,21);
    scene.actor_event(player_visual,0);
    scene.show_actor(pilot_visual,false);
    for(auto const handle : {crashes.land,crashes.water,crashes.contact}) scene.show_actor(handle,false);
    impact_projection.reset();
    radio.clear();
    // Native 0x7df30 announces a landed start before the first flight frame.
    if(mission_setup.start.landed) radio.show(tables.instruments().cleared_for_takeoff_message);
    camera_missile.reset();
    impact_aircraft.reset();
    radar = {};
    radar.nearby_hostile_count = options.nearby_hostile_count;
    seeker.begin_mission(options.projection_origin);
    designator = {};
    designator_mouse.reset();
    designator_view = designator_tracking_frame = false;
    designator_frame_gap = 0;
    views = tfx::cockpit::view_control{};
    view_motion = tfx::cockpit::view_camera{};
    external_orbit = tfx::cockpit::mouse_orbit{};
    view_pan = 0;
    cockpit_position.move_to(initial_cockpit_offset);
    ground_designation.reset();
    mobile_designation.reset();
    radiation_cursor = 0;
    player.aircraft = mission_setup.aircraft;
    auto const &airframe{tables.aerodynamic_airframes.at(std::to_underlying(player.aircraft) - 1)};
    // Original 8C184/95F3C run before 8B47C replaces the inventory. A new
    // external tank increases capacity without refilling airborne fuel;
    // cold-start placement fills it later. Keep the prior launch inputs.
    player_flight.initialise(airframe,launch_stores,{
      .position{vec3<int32_t>{mission_setup.start.position}},
      .angles{vec3<tfx::math::angle>{mission_setup.angles}},
      .model{preferences.model},
      .player_route_points{static_cast<unsigned int>(player_route.size())},
      .ground_elevation_steps{ground.height},
      .landed{mission_setup.start.landed},
      .wind{
        .terrain_wind_boost{environment.lighting.cloud_layer},
        .weather_wind_boost{environment.lighting.storm},
      },
      .surface_celsius{surface_celsius()},
    },mission_setup.speed_factor,wind_parameters,player_damage,options.session_random);
    // Original 95F3C/96411 clears the live wind setting, including a saved
    // REALISM=... wind enable. It can be enabled again from the flight menu.
    preferences.effects.wind = 0;
    player_render_basis = player.orientation;
    physical_player.powerplant.afterburner_control = render::display_page_count; // Shared native cockpit refresh clock.
    physical_player.mounted_stores = armament.flight_stores(airframe.stores_and_mounts.mounts);
    // The modal screen updates store statistics through 95DC8. Automatic
    // DEFAULT_WEAP fitting skips those previews until the first flight tick.
    if(!mission_setup.default_weapons)
      physical_player.stores = tfx::flight::accumulate_stores(airframe.stores_and_mounts.stores,physical_player.mounted_stores);
    // Native player-SSD setup 0x7d736 changes only the landed integer Y;
    // the float position and runway/carrier placement wait for the first update.
    if(mission_setup.start.landed) {
      int32_t constexpr initial_landed_visual_height{-3};
      player.integer_position.y = initial_landed_visual_height;
    }
    options.rain.initialise(options.session_random);
    player.crashing = preferences.effects.crashing != 0;
    // Native DATA 0x13594..0x1359c survives camera and mission resets.
    view_motion.initialise_ground_reference(options.ground_reference);
  }};
  auto const flight_audio{[&] {
    return audio::flight_audio_state{
      .throttle{player.throttle},
      .engines_running{player.left_engine || player.right_engine},
      .inhibited{ejection.value().stage != flight::ejection_stage::cockpit},
      .collision{player.collision != 0},
      .aircraft_disabled{player_disabled}
    };
  }};
  auto const advance_world{[&](tfx::flight::flight_controls const &controls, tfx::simulation::timer_ticks const elapsed, double const physics_seconds, bool const fire = false, bool const command_fire = false) {
    if(result_evaluated) return;
    ++designator_frame_gap;
    // Native 0x7205c gates animation before the player obtains this frame's time.
    auto const scene_update{scene.advance(previous_animation_ticks)};
    previous_animation_ticks = elapsed;
    launch_elapsed = elapsed;
    if(sound) { sound->advance(elapsed);sound->update_flight(flight_audio()); }
    auto const previous{vec3<int32_t>{player.integer_position}};
    bool const previous_gear{player.gear_down}, previous_chute{player.brake_chute};
    auto const fire_gun{[&](bool const trigger) {
      if(auto const shot{tfx::weapons::fire_cannon(cannon,{
        .aircraft{player.aircraft},
        .position{vec3<int32_t>{player.integer_position}},
        .angles{vec3<tfx::math::angle>{player.angles}},
        .speed{player.fixed_speed},
        .trigger{trigger},
        .gun_mode{armament.guns},
        .flaps_available{player_damage.condition(tfx::flight::aircraft_system::flaps) != tfx::flight::system_condition::unavailable},
        .communications_available{player_damage.condition(tfx::flight::aircraft_system::communications) != tfx::flight::system_condition::unavailable},
        .inhibited{refuelling.value().phase != 0},
        .crashed{player.collision != 0},
        .ejected{player_disabled || ejected()},
        .landed{player.landed},
      },tables.cannon_mounts,air_simulation.random_stream())}) {
        sample(audio::sample_priority::normal,audio::sample_id::cannon_fire);
        lapc_effect(audio::lapc_effect_cue::cannon_fire);
        projectiles.launch(*shot);
      }
    }};
    // Space and joystick fire share command 83FA8. The later held-Space
    // poll (86898) is separate, so an initial press can fire both barrels.
    if(command_fire && armament.guns && !player_disabled && !ejected() && !player.collision && !player.landed) {
      auto const condition{player_damage.condition(tfx::flight::aircraft_system::weapons)};
      fire_gun(condition != tfx::flight::system_condition::failed && (condition != tfx::flight::system_condition::intermittent || air_simulation.random_stream().next() <= 3000));
    }
    if(command_fire && !ejected() && !player_disabled && !armament.guns && armament.selected >= 0 && armament.selected < tfx::weapons::cannon_pylon && !player.landed) {
      auto const &weapon{tables.weapons.at(armament.pylons[static_cast<unsigned int>(armament.selected)].weapon)};
      // Native Space first acquires a ship for Harpoon; a subsequent press releases it. Laser stores require a designation.
      auto const status{player_damage.condition(tfx::flight::aircraft_system::weapons)};
      bool const trigger_available{status != tfx::flight::system_condition::failed && (status != tfx::flight::system_condition::intermittent || air_simulation.random_stream().next() <= 3000)};
      if(trigger_available && !player.collision && weapon.role == tfx::weapons::weapon_role::air) aiming.show_launch();
      bool acquired_ship{false};
      if(trigger_available && weapon.category == 13 && !ground_designation) {
        refresh_ground_contacts();
        if(auto const slot{tfx::sensors::nearest_ship(player.integer_position,ground_contacts,tables.ship_target_types)}) ground_designation.emplace(tfx::weapons::target_reference{
          .actor{tfx::weapons::actor_identity{
            .pool{tfx::weapons::actor_pool::surface},
            .index{surface_index(*slot)},
          }},
        });
        acquired_ship = true;
      }
      if(trigger_available && !acquired_ship && (weapon.role != tfx::weapons::weapon_role::ground || ground_designation || (weapon.category != 3 && weapon.category != 13))) {
        tfx::weapons::target_reference target{.untargeted{true}};
        if(weapon.role == tfx::weapons::weapon_role::ground && ground_designation) target = *ground_designation;
        else if(seeker.guidance) target = {
          .actor{tfx::weapons::actor_identity{
            .pool{tfx::weapons::actor_pool::aircraft},
            .index{*seeker.guidance},
          }},
        };
        tfx::weapons::selection_change change;
        armament.fire({
          .position{vec3<int32_t>{player.integer_position}},
          .angles{vec3<tfx::math::angle>{player.angles}},
          .speed{player.fixed_speed},
          .target{target},
          .inhibited{player.collision != 0 || refuelling.value().phase != 0},
          .unlimited{unlimited_stores},
        },tables.store_mounts[std::to_underlying(player.aircraft) - 1],change,emit_store);
        apply_selection(change);
      }
    }
    // Original 0x95dc8 reads each current weapon record, not just occupancy.
    // Rearming or restoring defaults can replace the weapon in a mount.
    physical_player.mounted_stores = armament.flight_stores(physical_player.aircraft.stores_and_mounts.mounts);
    physical_player.stores = tfx::flight::accumulate_stores(physical_player.aircraft.stores_and_mounts.stores,physical_player.mounted_stores);
    tfx::flight::aerodynamic_navigation_context navigation{
      .route_cells{player_route},
    };
    if(radar.selected && air_simulation.aircraft()[*radar.selected]) {
      auto const &target{air_simulation.aircraft()[*radar.selected]->flight.pose};
      navigation.followed_aircraft.emplace(tfx::flight::aerodynamic_follow_target{
        .position_q8{vec3<int32_t>{target.position}},
        .speed_q16{target.speed},
      });
    }
    auto const update{player_flight.advance(player_damage,air_simulation.random_stream(),wind_parameters,{
      .runway{landing.value()},
      .physics{
        .input{
          .source{simple_controls.source},
          .keys{simple_controls.keys},
          .joystick{vec2<int16_t>{simple_controls.joystick}},
          .mouse{designator_view ? vec2<int16_t>{0,0} : vec2<int16_t>{mouse_control.position}},
          .alt{simple_controls.alt},
          .accelerate{controls.accelerate},
          .decelerate{controls.decelerate},
        },
        .seconds{physics_seconds},
        .ground_elevation_steps{ground.height},
        .surface_celsius{surface_celsius()},
        .refuelling_phase{refuelling.value().phase},
        .terrain_present{ground.present},
        .rough_surface{ground_zone == 2},
        .carrier_surface{ground_zone == 3},
        .wheel_brakes{player.wheel_brake},
        .gear_damage_enabled{preferences.effects.gear_damage != 0},
        .precipitation{preferences.effects.precipitation != 0},
        .time_warp{acceleration.active},
        .crashing{player.crashing},
        .stick_pressure{stick_pressure},
        .wind_enabled{preferences.effects.wind != 0},
        .terrain_wind_boost{environment.lighting.cloud_layer},
        .weather_wind_boost{environment.lighting.storm},
        .navigation{navigation},
        .recovery_available{preferences.effects.recovery != 0},
        .ejected{player_disabled || ejected()},
        .approach_reference_enabled{landing.value().enabled},
      },
      .basic_controls{controls},
      .elapsed{elapsed},
      .surface_type{ground_zone},
      .pilot_terrain_type{scene.terrain_type(player.integer_position)},
      .refuel_per_update{tables.refuel_parameters().fuel_per_update},
      .aircraft_disabled{player_disabled},
      .water_world{environment.lighting.water_world},
    })};
    if(update.aerodynamic.ground.cockpit_bump) cockpit_position.bump();
    player_render_basis = player.orientation;
    if(preferences.effects.g_force != tfx::configuration::g_effects::none) tfx::render::advance_g_effects(g_effects,simple_player.displayed_g_load,preferences.effects.g_force,elapsed);
    else g_effects = {};
    if(update.horizon_motion) ground_phase.advance(*update.horizon_motion);
    bool const landing_callback{update.callback == tfx::flight::player_callback::automatic_landing};
    for(auto const id : update.gear_sounds) sample(audio::sample_priority::urgent,id);
    for(auto const change : update.aerodynamic.recovery_changes) if(change == flight::aerodynamic_recovery_change::engaged) sample(audio::sample_priority::urgent,audio::sample_id::recovery);
    if(update.refuel_afterburner_warning) radio.show(tables.refuel_parameters().afterburner_message);
    if(update.aerodynamic.descent_warning) {
      aiming.show_alert(tfx::cockpit::hud_alert::pull_up);
      sample(audio::sample_priority::urgent,audio::sample_id::pull_up);
    }
    for(auto const entry : update.model_entries) scene.actor_event(player_visual,entry);
    for(auto const event : update.aerodynamic.afterburner) {
      reheat_sound(event);
      if(event == tfx::flight::afterburner_change::started) scene.actor_event(player_visual,9);
      else if(event == tfx::flight::afterburner_change::stopped) scene.actor_event(player_visual,10);
    }
    for(auto const event : update.landing.ordered) {
      if(event == flight::automatic_landing_event::gear_lowered || event == flight::automatic_landing_event::gear_raised) {
        sample(audio::sample_priority::normal,audio::sample_id::gear);
        lapc_effect(audio::lapc_effect_cue::mechanical_switch);
      }
      else if(event == flight::automatic_landing_event::afterburner_stopped) sample(audio::sample_priority::normal,audio::sample_id::afterburner_stop);
      else if(event == flight::automatic_landing_event::stopped) sample(audio::sample_priority::urgent,audio::sample_id::landing_stopped);
      if(event == tfx::flight::automatic_landing_event::gear_lowered) scene.actor_event(player_visual,3);
      else if(event == tfx::flight::automatic_landing_event::gear_raised) scene.actor_event(player_visual,4);
      else if(event == tfx::flight::automatic_landing_event::afterburner_stopped) scene.actor_event(player_visual,10);
    }
    campaign["score"] = std::bit_cast<int32_t>(static_cast<uint32_t>(campaign["score"]) + static_cast<uint32_t>(update.landing.score_change));
    if(update.pilot_fatal) {
      mission_progress.outcome = tfx::missions::session_outcome::aircraft_lost;
      mission_progress.status = tfx::missions::session_status::aircraft_lost;
    }
    if(update.pilot_ended) mission_progress.ended = true;
    if(!ejected()) {
      if(!landing_callback && previous_gear != player.gear_down && player_damage.condition(tfx::flight::aircraft_system::landing_gear) != tfx::flight::system_condition::failed) {
        sample(audio::sample_priority::normal,audio::sample_id::gear);
        lapc_effect(audio::lapc_effect_cue::mechanical_switch);
      }
      if(!landing_callback && std::ranges::find(update.model_entries,player.gear_down ? 3u : 4u) == update.model_entries.end() && previous_gear != player.gear_down && player_damage.condition(tfx::flight::aircraft_system::landing_gear) != tfx::flight::system_condition::failed) {
        player_damage.systems[std::to_underlying(tfx::flight::aircraft_system::landing_gear)] = player.gear_down ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal;
        scene.actor_event(player_visual,player.gear_down ? 3u : 4u);
      }
      if(previous_chute && !player.brake_chute && std::ranges::find(update.model_entries,16u) == update.model_entries.end()) scene.actor_event(player_visual,16);
    }
    // Native 0x70ec8 follows player physics; the SSD event switches actors here,
    // leaving the first launch update to the next flight frame.
    if(ejection.advance_aircraft(player.position,body_basis(),player.fixed_speed,elapsed)) {
      player_visual = pilot_visual;
      scene.show_actor(aircraft_visual,false); // The departed aircraft has its own pre-terrain submission.
    }
    if(ejection.value().stage == tfx::flight::ejection_stage::pilot) scene.place_actor(aircraft_visual,vec3d{ejection.value().aircraft_position_q8} / tfx::actors::position_unit,ejection.value().aircraft_orientation.angles());
    // Native 0x86898 fires the held Space key after its player-physics callback, before other actors update.
    // Original nonzero checks read FLP and COM slots, despite their unrelated display names.
    fire_gun(fire);
    // Native 0x720b8 follows player physics/cannon, then 0x71b40 sends timed
    // messages. Suspension bumps apply now; later flak starts next frame.
    cockpit_position.advance(tables.instruments().viewports[view_index()].bottom,!designator_view && views.value().current == tfx::cockpit::view_mode::cockpit);
    for(auto const index : mission_time.begin_frame()) radio.show(mission_setup.messages[index].text);
    // Native 871D4 precedes completion and ground service. A refill repairs
    // conditions afterwards; its existing repair clocks survive until next tick.
    tfx::flight::repair_systems(player_damage,elapsed);
    std::vector<tfx::missions::target_life> targets;
    for(auto const &target : mission_targets) targets.push_back(target_intact(target) ? tfx::missions::target_life::alive : tfx::missions::target_life::dead);
    for(auto const &event : mission_progress.begin_frame({
      .mission_time{mission_time.elapsed()},
      .targets{targets},
      .landed{player.landed},
      .speed{player.fixed_speed},
      .arcade{arcade_rules},
      .airspace{tfx::missions::airspace_context{
        .position{player.integer_position.x,player.integer_position.z},
        .acceleration_active{acceleration.active},
        .radio_idle{radio.status().complete},
      }},
    })) {
      if(auto const *notice{std::get_if<missions::progress_notice>(&event)}) radio.show(notice->kind == missions::notice_kind::target_destroyed ? *mission_setup.targets[notice->target].text : *mission_setup.completion->text,notice->kind != missions::notice_kind::target_destroyed);
      else if(auto const *boundary{std::get_if<missions::airspace_event>(&event)}) {
        if(boundary->cancel_acceleration) { acceleration.cancel();radio.stop(); }
        if(boundary->notice != missions::airspace_notice::none) radio.show(tables.instruments().airspace_messages[std::to_underlying(boundary->notice) - 1],boundary->notice == missions::airspace_notice::departed);
      } else {
        if(sound) sound->music_cue(audio::music_cue_id::mission_complete);
        sample(audio::sample_priority::urgent,audio::sample_id::mission_complete);
      }
    }
    // Native 71E44 runs after objective/landing checks and before other actors.
    if(ground_service.begin_frame({
      .landed{player.landed},
      .speed{player.fixed_speed},
      .resolved{mission_progress.resolved},
      .crashed{player.collision != 0},
      .disabled{player_disabled || ejected()},
      .violations{mission_progress.violations},
    })) {
      if(sound) sound->music_cue(audio::music_cue_id::normal_flight);
      player.fuel = physical_player.powerplant.fuel = physical_player.stores.fuel_capacity;
      campaign["score"] = std::bit_cast<int32_t>(static_cast<uint32_t>(campaign["score"]) + static_cast<uint32_t>(flight::ground_service_reward));
      if(mission_setup.default_weapons) armament.fit_default(mission_setup.weapons);
      else {
        if(!active_window) throw std::runtime_error{"ground rearming requires a window for the original arming screen"};
        glfwSetInputMode(active_window->get(),GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        if(arm_mission(*active_window,game,tables,armament,player.aircraft,mission_setup.weapons,tfx::presentation::arming_entry::mission_incomplete) == arming_result::closed) return;
        auto const &thumbnail{tables.arming_art().rows.back()};
        options.projection_origin = (thumbnail.minimum + thumbnail.maximum) / int16_t{2};
        service_modal_closed = true;
      }
      reset_weapon_targets();
      uint8_t constexpr restored_countermeasures{15};
      defences.chaff = defences.flares = restored_countermeasures;
      landing.disengage();
      cannon.ammunition = tfx::weapons::initial_cannon_ammunition;
      radio.show(tables.instruments().ground_service_message);
      scene.actor_event(player_visual,14);
      scene.actor_event(player_visual,0);
      player_damage.systems.fill(flight::system_condition::normal);
      player_damage.systems[std::to_underlying(flight::aircraft_system::landing_gear)] = flight::system_condition::active;
      // 8FAD4 restores BASIC Mach 1 and snaps heading to a cardinal direction.
      // 8C184/8C318 reuse the authored initial-ground flag, even after a landing.
      math::angle constexpr heading_bias{0x1000}, cardinal_mask{0xc000};
      auto const heading{static_cast<math::angle>(static_cast<math::angle>(player.angles.y + heading_bias) & cardinal_mask)};
      auto const retained_approach{automatic_landing};
      auto const retained_navigation{physical_player.autopilot.navigation};
      auto const retained_guidance{physical_player.autopilot.guidance};
      auto const &aircraft{tables.aerodynamic_airframes.at(std::to_underlying(player.aircraft) - 1)};
      auto const stores{armament.flight_stores(aircraft.stores_and_mounts.mounts)};
      player_flight.initialise(aircraft,stores,{
        .position{vec3<int32_t>{player.integer_position}},
        .angles{vec3<math::angle>{0,heading,0}},
        .model{preferences.model},
        .player_route_points{static_cast<unsigned int>(player_route.size())},
        .ground_elevation_steps{ground.height},
        .landed{mission_setup.start.landed},
        .wind{
          .terrain_wind_boost{environment.lighting.cloud_layer},
          .weather_wind_boost{environment.lighting.storm},
        },
        .surface_celsius{surface_celsius()},
      },1.0f,wind_parameters,player_damage,air_simulation.random_stream());
      preferences.effects.wind = 0; // The same 96411 reset runs after servicing.
      automatic_landing = retained_approach; // Shared approach scratch survives 8FAD4.
      // 8FAD4/95F3C retain route and guidance scratch, including the coordinates
      // shared with the approach callback. This is not a new mission launch.
      physical_player.autopilot.navigation = retained_navigation;
      physical_player.autopilot.guidance = retained_guidance;
      physical_player.powerplant.afterburner_control = render::display_page_count; // Rearming resets the viewport too.
    }
    ground_service.finish_frame(player.landed);
    // Native 75784/75114 publishes the shared player contact after servicing,
    // before the actor ticks. Earlier input handlers see its previous value,
    // including the zeroed cold-start record and retention between missions.
    options.player_contact = tfx::actors::air_contact{
      .position{vec3<int32_t>{vec3<long double>{player.position} * tfx::actors::position_unit}},
      .angles{vec3<tfx::math::angle>{player.angles}},
      .speed{std::bit_cast<int32_t>(static_cast<uint32_t>(player.fixed_speed) << tfx::actors::speed_fraction_bits)},
    };
    air_world.player = options.player_contact;
    scene.place_actor(player_visual,vec3d{player.position},player.angles);
    advance_actors(elapsed);
    mission_progress.advance_return_countdown(elapsed,arcade_rules);
    if(mission_progress.force_landed_return) {
      player.fixed_speed = 0;
      player.landed = true;
    }
    auto const contact_source{player_visual};
    auto const contact_damage{scene.actor_impact_damage(contact_source)};
    {
      // Native 0x72018 updates actors before collecting contacts; the next flight frame consumes this surface result.
      bool const previously_destroyed{scene.actor_destroyed(player_visual)};
      bool const contact_explosion{player_visual == crashes.contact};
      if(previously_destroyed && !contact_explosion) scene.actor_event(player_visual,0);
      auto const response{[&](uint16_t const cost) { scene.apply_actor_contact_damage(contact_source,cost);damage_player(100); }};
      // Main-loop 7210A/72111 clear both outputs before collecting this
      // frame's contacts. A height-only SSD write can replace zero afterwards.
      ground.present = false;
      ground.height = 0;
      ground_zone = -1;
      auto const collect{[&](tfx::world::instance_result const &contact) {
        if(contact.zone != -1) ground_zone = contact.zone;
        if(contact.surface_present) ground.present = true;
        if(contact.surface_height) ground.height = *contact.surface_height;
      }};
      for(auto const handle : fixed_scenery) collect(scene.collide_actor(handle,previous,player.integer_position,contact_damage,response,damage_player,true));
      collect(scene.collide_terrain(previous,player.integer_position,contact_damage,true,response));
      // Native 0x7c724 queries nearby mobile assemblies after the terrain, including carrier decks.
      auto const region{tfx::actors::nearby_surface_region(previous,player.integer_position)};
      for(auto const &actor : surface_actors) if(!(actor.state.pose.flags & std::to_underlying(tfx::actors::surface_flag::retired)) && region.contains(actor.state.pose.position)) collect(scene.collide_actor(actor.visual,previous,player.integer_position,contact_damage,response,damage_player,true));
      // Native 0x7b094 follows terrain/mobile contacts, oldest transient first.
      for(auto const slot : effects.active_slots()) {
        auto const &effect{effects.record(slot)};
        auto const visual{effect_visuals[slot][std::to_underlying(effect.kind)]};
        scene.place_actor(visual,vec3d{tfx::world::effect_position(effect.position,player.integer_position)},{0,0,0});
        collect(scene.collide_actor(visual,previous,player.integer_position,contact_damage,response,damage_player,true));
      }
      auto const contact{tfx::world::resolve_player_contact({
        .previously_destroyed{previously_destroyed},
        .destroyed{scene.actor_destroyed(player_visual)},
        .contact_explosion{contact_explosion},
        .pilot_active{ejection.value().pilot.phase != tfx::flight::pilot_phase::inactive},
      })};
      if(contact.collision) player.collision = 1;
      if(contact.release_motion_hold) player.frozen = false;
      if(contact.action == tfx::world::player_contact_action::restore_destroyed) scene.actor_event(player_visual,17);
      else if(contact.action == tfx::world::player_contact_action::explode) {
        mission_progress.begin_contact_explosion();
        views.enqueue(tfx::cockpit::view_mode::unbanked_chase,player.fixed_speed);
        scene.show_actor(player_visual,false);
        player_visual = crashes.contact;
        scene.actor_event(player_visual,0);
        scene.place_actor(player_visual,vec3d{player.position},player.angles);
        sample(audio::sample_priority::normal,audio::sample_id::aircraft_explosion);
      }
    }
    // Native 0x71535 checks only the selected refueller after ordinary world contacts.
    bool const before_tanker{scene.actor_destroyed(player_visual)};
    if(auto const selected{refuelling.value().selected}) {
      unsigned int const index{tfx::actors::aircraft_per_side + *selected};
      if(air_simulation.aircraft()[index] && air_visuals[index]) {
        auto const response{[&](uint16_t const cost) { scene.apply_actor_contact_damage(contact_source,cost);damage_player(100); }};
        scene.collide_actor(*air_visuals[index],previous,player.integer_position,contact_damage,response,damage_player,true);
        if(!before_tanker && scene.actor_destroyed(player_visual)) {
          scene.actor_event(player_visual,17);
          fatal_aircraft(index,tfx::actors::air_loss_cause::collision);
        }
      }
    }
    {
      // Native 0x7d42b resets the player SSD every cheat frame until loss/
      // ejection. Collision and already-started aftermath still run below.
      if(options.cheat.value().enabled && !player_disabled) scene.actor_event(player_visual,0);
      // Native 7D3F8 publishes this record only on the ordinary loss path.
      // The earlier kapow/contact-explosion branch skips it (7D298).
      if(player_visual != crashes.contact && (!options.cheat.value().enabled || player_disabled)
        && (scene.actor_destroyed(player_visual) || player.collision)) {
        disabled_camera_position_q8 = vec3<int32_t>{vec3<uint32_t>{player.integer_position} * static_cast<uint32_t>(tfx::actors::position_unit)};
      }
      auto const loss{mission_progress.observe_player_loss({
        .fatal{scene.actor_destroyed(player_visual)},
        .collision{player.collision != 0},
        .elapsed{elapsed},
        .invulnerable{options.cheat.value().enabled},
        .height{player.integer_position.y},
        .pilot_active{ejection.value().pilot.phase != tfx::flight::pilot_phase::inactive},
        .contact_explosion{player_visual == crashes.contact},
      })};
      if(loss.force_collision) player.collision = 1;
      if(loss.first_loss) {
        if(sound) sound->music_cue(audio::music_cue_id::aircraft_lost);
        scene.actor_event(player_visual,21);
        views.start_impact_orbit(player.fixed_speed);
      }
      if(loss.explosion && sound) {
        if(loss.mayday) sound->sequence(audio::sample_priority::normal,{std::to_underlying(audio::sample_id::aircraft_explosion),std::to_underlying(audio::sample_id::mayday)});
        else sample(audio::sample_priority::normal,audio::sample_id::aircraft_explosion);
      }
      if(loss.first_impact) {
        bool const water_world{environment.lighting.water_world};
        auto const point{impact_projection ? impact_projection->integer(player.integer_position) : std::nullopt};
        auto const surface{tfx::world::classify_impact(renderer.output,point,water_world)};
        auto const appearance{tfx::world::select_crash_appearance(player.collision,scene.terrain_type(player.integer_position),surface,water_world)};
        scene.show_actor(player_visual,false);
        player_visual = appearance == tfx::world::crash_appearance::water ? crashes.water : crashes.land;
        scene.actor_event(player_visual,0);
        player.angles = {0,0,0};
        scene.place_actor(player_visual,vec3d{player.position},player.angles);
      }
    }
    projectiles.advance(elapsed,air_simulation.random_stream());
    sky_animation = scene_update.animated;
    flak_flash = false;
    if(auto const event{tfx::world::advance_flak(flak_state,{
      .player{vec3<int32_t>{player.integer_position}},
      .regions{mission_setup.flak},
      .night{night_flak},
      .cloud_layer{environment.lighting.cloud_layer},
      .layer_y{environment.lighting.layer_y},
      .radio_idle{radio.status().complete}, // Native 0x7612 is the ticker's idle gate.
    })}) {
      if(event->sample) sample(audio::sample_priority::normal,audio::sample_id::nearby_flak);
      if(event->radio) radio.show(tables.flak_warning);
      flak_flash = event->flash;
      if(event->flash) cockpit_position.shake();
      auto const slot{flak.spawn(*event)};
      scene.actor_event(flak_visuals[slot].at(event->model),0);
    }
    update_sensor_inputs();
    mission_time.advance_clock(elapsed);
  }};
  auto const finish_world{[&](tfx::simulation::timer_ticks const elapsed) {
    if(result_evaluated) return;
    for(auto const index : mission_time.finish_frame()) {
      auto const &event{mission_setup.scenery_events[index]};
      scene.terrain_event(event.cell.x,event.cell.y,event.entry);
    }
    // Native 0x70e44 updates player streams after all world/HUD drawing.
    if(scene.animate_player_model(player_visual)) ejection.request();
    if(mission_progress.finish_frame(elapsed)) radio.show(tables.timer_expired_message);
    if(mission_progress.ended) {
      auto const recovered{recovery.resolve({
        .outcome{mission_progress.outcome},
        .status{mission_progress.status},
        .safe{campaign.contains("safe") && campaign.at("safe") != 0},
      },{
        .ejected{ejected()},
        .fatal{ejection.value().pilot.fatal},
        .damaged{player_damage.accumulated != 0},
        .violations{mission_progress.violations},
        .region{tfx::missions::pilot_recovery_region(selected.world_name)},
        .start{mission_setup.start.position.x,mission_setup.start.position.z},
        .position{player.integer_position.x,player.integer_position.z},
      })};
      mission_progress.outcome = recovered.outcome;
      mission_progress.status = recovered.status;
      campaign["safe"] = recovered.safe;
      campaign["score"] = std::bit_cast<int32_t>(static_cast<uint32_t>(campaign["score"]) + static_cast<uint32_t>(mission_progress.return_score));
      campaign["outcome"] = std::to_underlying(mission_progress.outcome);
      campaign["status"] = std::to_underlying(mission_progress.status);
      mission_finish.execute(mission_progress.results,campaign);
      result_evaluated = true;
      std::cout << "Mission result: " << campaign["miss_val"] << "; score " << campaign["score"] << "; outcome " << campaign["outcome"] << std::endl;
    }
  }};
  if(!inspect || flight_steps || flight_frames) restart();
  mission_handoff const remember{inspect ? nullptr : &options.mission_state,player,mission_setup.weapons};
  if(sound && !inspect && !override_cell && !screenshot) sound->begin_flight(flight_audio());
  for(unsigned int step{0}; step != flight_steps; ++step) {
    advance_world({},4,static_cast<double>(4 * .02f));
    finish_world(4);
    projectiles.finish_frame();
    std::erase_if(trail_samplers,[&](auto const &entry) { return !projectiles.projectiles().contains(entry.first); });
  }
  // Native 0x72588 resumes the parser directly after FINISH and teardown.
  // A direct --mission launch keeps its standalone diagnostic result display.
  if(frontend_flight && result_evaluated) return flight_result::finished;
  vec3d position{mission_setup.start.position};
  if(mission_setup.start.landed) position.y = -300;
  double heading{0}, pitch{-.12};
  if(override_cell) {
    inspect = true;
    position = vec3d{(override_cell->x + .5) * tfx::world::cell_units,-1800,(override_cell->y + .5) * tfx::world::cell_units};
    pitch = -.35;
  }
  bool paused{false}, paused_camera_update{false};
  tfx::recording::video_recorder flight_recorder;
  std::optional<double> recording_timestamp;
  auto const queue_projectiles{[&](vec3<int32_t> const &camera_anchor) {
    int32_t constexpr maximum_projectile_draw_distance{10000};
    // Native 0xa30cc walks newest first, including the final damaged frame.
    for(auto const &[_,shot] : projectiles.projectiles() | std::views::reverse) {
      if(tfx::math::distance_estimate(projectile_world_position(shot->state.position),camera_anchor) >= maximum_projectile_draw_distance) continue;
      scene.queue_actor(weapon_visuals[shot->state.weapon][shot->state.secondary],vec3d{shot->state.position} / tfx::actors::position_unit,shot->state.angles,&camera_anchor,tfx::world::submission_phase::before_actors);
    }
  }};
  auto const draw_instruments{[&](std::optional<std::string> const &radio_window) {
    render::preview_state instrument_frame{instruments,paused};
    render::preview_state scenes_frame{scene_displays,paused};
    render::preview_state selection_frame{display_selection,paused};
    render::preview_state random_frame{air_simulation.random_stream(),paused};
    std::array<std::vector<tfx::cockpit::radar_aircraft>,2> air_contacts;
    std::array<std::vector<tfx::cockpit::radar_surface>,2> surface_contacts;
    for(unsigned int pool{0}; pool != air_contacts.size(); ++pool) for(unsigned int reverse{tfx::actors::aircraft_per_side}; reverse; --reverse) {
      unsigned int const index{pool * tfx::actors::aircraft_per_side + reverse - 1};
      auto const &actor{air_simulation.aircraft()[index]};
      if(!actor || (actor->flight.pose.flags & std::to_underlying(tfx::actors::air_flag::retired))) continue;
      air_contacts[pool].push_back({
        .position_q8{vec3<int32_t>{actor->flight.pose.position}},
        .selected{radar.selected == index},
      });
    }
    for(unsigned int reverse{static_cast<unsigned int>(surface_actors.size())}; reverse; --reverse) {
      auto const &actor{surface_actors[reverse - 1]};
      if(!visible_surface_contact(actor)) continue;
      unsigned int const pool{std::to_underlying(actor.state.pose.side)};
      surface_contacts[pool].push_back({
        .position_q8{vec3<int32_t>{actor.state.pose.position}},
        .type{scene.actor_type(actor.visual)},
      });
    }
    std::vector<tfx::cockpit::radar_projectile> radar_projectiles;
    // Native 82FBE walks the intrusive list newest first. Overlapping player
    // and enemy missile markers retain that order, independently of identity.
    // Impacted records still appear until the normal frame-tail retirement.
    for(auto const &[_,shot] : projectiles.projectiles() | std::views::reverse) radar_projectiles.push_back({
      .position_q8{vec3<int32_t>{shot->state.position}},
      .weapon{static_cast<tfx::weapons::weapon_id>(shot->state.weapon)},
      .player_owned{shot->owner.pool == tfx::weapons::actor_pool::player},
    });
    tfx::cockpit::hud_state state;
    state.threat = radar.hostile_nearby;
    state.fire_permission_notice = engagement.notice_frames != 0;
    if(launch_warning) state.tracked_weapon_category = tables.weapons[launch_warning->state.weapon].category;
    auto &readings{state.instruments};
    readings.aircraft = player.aircraft;
    readings.offset = cockpit_frame.clamp(tfx::cockpit::forward_cockpit,cockpit_position.value());
    readings.systems = player_damage.systems;
    auto const switched_system{[&](tfx::flight::aircraft_system const system, bool const active) {
      auto &condition{readings.systems[std::to_underlying(system)]};
      if(condition != tfx::flight::system_condition::failed && condition != tfx::flight::system_condition::intermittent) condition = active ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal;
    }};
    switched_system(tfx::flight::aircraft_system::landing_gear,player.gear_down);
    switched_system(tfx::flight::aircraft_system::wheel_brakes,player.wheel_brake);
    switched_system(tfx::flight::aircraft_system::air_brake,player.air_brake);
    readings.speed = player.fixed_speed;
    readings.height = player.position.y;
    readings.heading = player.angles.y;
    readings.fuel = player.fuel;
    readings.fuel_gauge = fuel_display.reading;
    readings.smoothed_fuel_burn = fuel_display.smoothed_burn;
    readings.throttle = player.throttle;
    readings.engine_load = {physical_player.powerplant.engine_factor.x,physical_player.powerplant.engine_factor.y};
    auto const &targets{physical_player.manual.controls.autopilot};
    readings.controls = {
      .trailing_edge_flap{physical_player.manual.flap_fraction},
      .leading_edge_flap{physical_player.surfaces.automatic_flap_fraction},
      .thrust_vector{vec2d{physical_player.powerplant.nozzle_angles}},
      .autopilot{
        .enabled{targets.enabled},
        .mode{static_cast<tfx::cockpit::autopilot_mode>(targets.mode)},
        .heading{targets.heading},
        .waypoint{physical_player.autopilot.navigation.route_index},
        .height{targets.down_metres},
        .speed{targets.metres_per_second},
      },
      .configuration{static_cast<tfx::cockpit::flight_configuration>(physical_player.autopilot.recovery.configuration)},
    };
    readings.inventory = &armament;
    readings.confirmed_target = seeker.guidance.has_value() || ground_designation.has_value();
    state.position = vec3<int32_t>{player.integer_position};
    state.ticks = mission_time.elapsed();
    state.clock_offset = static_cast<uint32_t>(mission_setup.clock);
    state.elapsed = paused ? 0 : launch_elapsed;
    state.chaff = defences.chaff;
    state.flares = defences.flares;
    state.ecm = defences.ecm;
    state.stealth = defences.stealth;
    state.stall_recovery = player.recovering;
    state.hook = player.arrestor_hook;
    state.redraw_phase = physical_player.powerplant.afterburner_control;
    state.afterburner_pips = player.afterburner;
    state.low_speed_ground_contact = player.low_speed_ground_contact;
    state.landed = player.landed;
    state.contacts = {
      .enemy_aircraft{air_contacts[0]},
      .allied_aircraft{air_contacts[1]},
      .enemy_surface{surface_contacts[0]},
      .allied_surface{surface_contacts[1]},
      .projectiles{radar_projectiles},
    };
    state.targets = map_targets;
    if(landing.value().enabled) state.landing_cell.emplace(vec2<int16_t>{landing.value().cell});
    std::optional<vec3<int32_t>> range_target;
    if(ground_designation) range_target.emplace(designated_point.x >> 8,designated_point.y >> 8,designated_point.z >> 8);
    else if(radar.selected && air_simulation.aircraft()[*radar.selected]) {
      auto const &target{air_simulation.aircraft()[*radar.selected]->flight.pose.position};
      range_target.emplace(target.x >> 8,target.y >> 8,target.z >> 8);
    }
    // Native 0x80320 reads the selected pylon even in cannon/NAV mode.
    unsigned int const range_weapon_index{armament.selected < 0 ? 0u : armament.pylons[static_cast<unsigned int>(armament.selected)].weapon};
    auto const &range_weapon{tables.weapons.at(range_weapon_index < tables.weapons.size() ? range_weapon_index : 0u)};
    if(range_target) {
      int32_t const distance{tfx::math::distance_estimate(player.integer_position,*range_target)};
      state.in_range = distance != 0 && distance < range_weapon.range;
    }
    if(radio_window) state.radio_message = *radio_window;
    auto const result{instrument_frame->draw(renderer,selection_frame.value(),state,random_frame.value(),nullptr,[&](tfx::cockpit::display_side const side, tfx::cockpit::display_page const page, tfx::cockpit::mfd_state const &display) {
      tfx::cockpit::scene_camera_state camera_state{
        .aircraft{player.aircraft},
        .position{vec3<int32_t>{player.integer_position}},
        .angles{vec3<tfx::math::angle>{player.angles}},
        .free_angles{designator_view ? vec2<tfx::math::angle>{designator.pitch,designator.heading} : vec2<tfx::math::angle>{player.angles.x,player.angles.y}},
        .tracked_position_q8{},
        .tracking_roll{0},
        .ground_target{0,0,0},
        .height_bounds{vec2<int32_t>{movement_height_bounds}},
      };
      if(ground_designation) camera_state.tracked_position_q8.emplace(vec3<int32_t>{designated_point});
      if(!mission_setup.targets.empty()) camera_state.ground_target = vec3<int32_t>{mission_setup.targets[ground_target_page].position};
      auto const previous_clipping{renderer.clipping};
      scenes_frame->draw(renderer,side,page,display.offset,camera_state,[&](tfx::cockpit::scene_camera_request const &request) {
        vec2<int> const minimum{std::max(0,request.minimum.x),std::max(0,request.minimum.y + (request.maximum.y < tfx::render::screen_height ? 1 : 0))};
        vec2<int> const maximum{std::min(tfx::render::screen_width - 1,request.maximum.x),std::min(tfx::render::screen_height - 1,request.maximum.y - 1)};
        renderer.clipping = tfx::render::raster_clip::pixels(minimum,maximum);
        auto const region{tfx::render::instrument_camera(request.view(),video,minimum,maximum)};
        if(!region) return vec2<int16_t>{0,0};
        unsigned int const slot{side == tfx::cockpit::display_side::left ? 0u : 1u};
        auto &camera_renderer{instrument_renderers[slot]};
        if(camera_renderer.output.size != region->size) camera_renderer.output.resize(region->size);
        auto const &camera{region->view};
        camera_renderer.clipping = tfx::render::raster_clip::pixels({0,0},vec2<int>{camera.viewport_maximum});
        tfx::render::paint_horizon(camera_renderer.output,camera,ground_phase.sample());
        queue_projectiles(request.position);
        select_actor_draw_region(request.position);
        options.projection_origin = vec2<int16_t>{request.view().projection_origin};
        scene.set_visual_lod_scale(video.lod_scale);
        auto const rendered{scene.draw(camera_renderer,camera,scene_radius,[&] { queue_effects(camera); },request.position.y,ground.height,tfx::world::scene_pass::instrument)};
        if(!paused) clean_trails();
        if(!paused && rendered.eject_player) ejection.request();
        instrument_positions[slot].emplace(vec2<int>{region->position});
        // Punch out the old UI background here. Reticles and range text drawn
        // after this callback acquire coverage normally, above the 3D image.
        for(int y{minimum.y}; y <= maximum.y; ++y) for(int x{minimum.x}; x <= maximum.x; ++x) renderer.output.coverage[static_cast<size_t>(y * tfx::render::screen_width + x)] = 0;
        if(!request.tracked_position) return vec2<int16_t>{0,0};
        auto const point{camera.transform(vec3d{*request.tracked_position})};
        if(point.z <= 0) return vec2<int16_t>{-32000,-32000};
        tfx::render::interface_layout const layout{video.resolution};
        auto const screen{(camera.project(point) + vec2d{region->position} - layout.offset) / layout.scale};
        return vec2<int16_t>{static_cast<int16_t>(std::clamp(screen.x,-32768.0,32767.0)),static_cast<int16_t>(std::clamp(screen.y,-32768.0,32767.0))};
      });
      renderer.clipping = previous_clipping;
    },&hud_vectors)};
    physical_player.powerplant.afterburner_control = instruments.animation().redraw_phase;
    if(!paused) player_damage.systems[std::to_underlying(tfx::flight::aircraft_system::fuel)] = result.fuel_condition;
    if(!paused && result.threat_sound) sample(audio::sample_priority::urgent,audio::sample_id::threat);
  }};
  auto const draw_aiming{[&](tfx::render::camera const &view, bool const limited) {
    render::preview_state aiming_frame{aiming,paused};
    render::preview_state random_frame{air_simulation.random_stream(),paused};
    render::preview_state landing_frame{landing,paused};
    aiming_frame->check_fuel(player.fuel);
    auto const fault{player_damage.condition(tfx::flight::aircraft_system::hud)};
    if(fault == tfx::flight::system_condition::failed || (fault == tfx::flight::system_condition::intermittent && (random_frame->next() & 1))) return;
    // Original 0x8015b scans after the seeker update (0x7213f), only while
    // drawing a working forward HUD. Newly found contacts are acquired next frame.
    if(!paused) {
      auto const scan{radar.update(player.integer_position,radar_actors)};
      options.nearby_hostile_count = radar.nearby_hostile_count;
      if(scan.more_hostiles && radio.status().complete) radio.show(tables.instruments().nearby_fighters_message);
      if(scan.announced_actor && sound) {
        auto const &actor{*std::ranges::find(radar_actors,*scan.announced_actor,&tfx::sensors::air_contact::actor)};
        vec3<int32_t> const player_q8{vec3<uint32_t>{player.integer_position} * static_cast<uint32_t>(tfx::actors::position_unit)};
        int constexpr angle_units_per_degree{182}, feet_per_thousand{1000};
        auto const bearing{static_cast<int16_t>(tfx::math::bearing(player_q8,actor.position).y / angle_units_per_degree)};
        auto const altitude{tfx::math::signed_word(static_cast<uint16_t>(-(actor.position.y >> tfx::actors::position_fraction_bits) * tfx::flight::feet_per_world_unit / feet_per_thousand))};
        sound->sequence(audio::sample_priority::urgent,audio::radar_report_voice(bearing,altitude));
        sound->music_cue(audio::music_cue_id::normal_flight);
      }
    }
    auto const offset{vec2<int16_t>{cockpit_position.value()}};
    unsigned int const weapon_index{armament.selected < 0 ? 0u : armament.pylons[static_cast<unsigned int>(armament.selected)].weapon};
    auto const &weapon{tables.weapons.at(weapon_index < tables.weapons.size() ? weapon_index : 0)};
    // Native 0x801cd selects NAV from the gun/cannon flags; 0x7fc11 then
    // selects the weapon layout from the current role, including automatic
    // post-release and jettison selection changes.
    auto const role{armament.selected_role()};
    auto const mode{armament.guns && (armament.selected != tfx::weapons::cannon_pylon || cannon_navigation)
      ? tfx::cockpit::aiming_mode::navigation
      : role == tfx::weapons::weapon_role::air ? tfx::cockpit::aiming_mode::air
      : role == tfx::weapons::weapon_role::ground ? tfx::cockpit::aiming_mode::ground
      : tfx::cockpit::aiming_mode::stores};
    tfx::cockpit::weapon_display_state state{
      .mode{mode},
      .aircraft{player.aircraft},
      .weapon{weapon.label},
      .range{weapon.range},
      .category{weapon.category},
      .cannon{weapon_index == 0},
      .ammunition{cannon.ammunition},
      .suppressed{seeker.too_close},
      .air_lock{seeker.guidance.has_value() || (weapon.category == tfx::weapons::cruise_missile_category && ground_designation.has_value())},
      .ground_lock{ground_designation.has_value()},
      .limited{limited},
      .elapsed{paused ? tfx::simulation::timer_ticks{0} : launch_elapsed},
      .attitude{
        .angles{vec3<tfx::math::angle>{player.angles}},
        .projection_angles{static_cast<tfx::math::angle>(-player.angles.x),static_cast<tfx::math::angle>(-player.angles.z)},
        .projection_origin{static_cast<int16_t>(view.projection_origin.x),static_cast<int16_t>(view.projection_origin.y)},
        .offset{vec2<int16_t>{offset}},
        .focal_distance{view.point_focal_distance.value_or(view.focal_distance)},
      },
      .flight{
        .height{player.position.y},
        .speed{player.fixed_speed},
        .mach{player.unrounded_speed_mach},
        .load{simple_player.displayed_g_load},
        .throttle{player.throttle},
        .position{vec3<int32_t>{player.integer_position}},
        // Native 7EB7C reads retained deployment, independently of the switch.
        .air_brake{physical_player.manual.air_brake_fraction != 0},
        .brake_chute{player.brake_chute},
        .offset{vec2<int16_t>{offset}},
      },
    };
    if(preferences.model == tfx::simulation::flight_model::military_spec || preferences.model == tfx::simulation::flight_model::realistic) state.aerodynamic.emplace(tfx::cockpit::aerodynamic_hud_state{
      .angle_of_attack{physical_player.data.angle_of_attack},
      .down_metres_per_second{physical_player.response.motion.velocity_ned.z},
    });
    if(radar.selected && air_simulation.aircraft()[*radar.selected]) {
      auto const &actor{air_simulation.aircraft()[*radar.selected]->flight.pose};
      auto const profile{std::ranges::find(profiles,actor.profile_id,&tfx::actors::profile::id)};
      state.air_target.emplace(tfx::cockpit::air_target_cue{
        .reading{
          .name{profile->name},
          .position_q8{vec3<int32_t>{actor.position}},
          .heading{actor.angles.y},
          .speed_q16{actor.speed},
        },
        .orientation{body_basis()},
        .projected{vec2<int16_t>{seeker.projected}},
        .tracking_q8{vec2<int32_t>{seeker.reticle}},
        .acquiring{seeker.tracking.has_value()},
      });
    }
    if(refuelling.value().selected) {
      unsigned int const index{tfx::actors::aircraft_per_side + *refuelling.value().selected};
      auto const &actor{air_simulation.aircraft()[index]->flight.pose};
      auto const profile{std::ranges::find(profiles,actor.profile_id,&tfx::actors::profile::id)};
      state.navigation_target.emplace(tfx::cockpit::air_target_cue{
        .reading{
          .name{profile->name},
          .position_q8{vec3<int32_t>{actor.position}},
          .heading{actor.angles.y},
          .speed_q16{actor.speed},
        },
        .orientation{body_basis()},
        // The original refuelling HUD precedes acquisition and shares its
        // retained projected point, independently of the selected actor data.
        .projected{vec2<int16_t>{seeker.projected}},
      });
    }
    auto const &pose{view_motion.last_pose()};
    tfx::render::point_projection const projection{
      .matrix{mat3<int32_t>{pose.matrix}},
      .position_q16{vec3<int32_t>{pose.position_q16}},
      .origin{static_cast<int16_t>(view.projection_origin.x),static_cast<int16_t>(view.projection_origin.y)},
      .focal_distance{view.point_focal_distance},
    };
    if(!mission_setup.targets.empty()) {
      auto const &target{mission_setup.targets[ground_target_page].position};
      auto const point{projection.integer({target.x,0,target.z})};
      state.ground_target.emplace(tfx::cockpit::ground_target_cue{
        .position{target.x,0,target.z},
        .display{
          .player{vec3<int32_t>{player.integer_position}},
          .orientation{body_basis()},
          .projected{point ? vec2<int16_t>{*point} : vec2<int16_t>{-1,-1}},
          .offset{vec2<int16_t>{offset}},
        },
      });
    }
    if(state.mode == tfx::cockpit::aiming_mode::navigation && landing.value().enabled) {
      state.landing.emplace(landing_frame->advance(player.integer_position,false,[&](auto const &position) {
        if(auto const point{projection.integer(position)}) return tfx::flight::landing_projection{
          .valid{true},
          .point{vec2<int16_t>{*point}},
        };
        return tfx::flight::landing_projection{};
      }));
      if(!paused && state.landing->speed_request) radio.show(tables.instruments().landing_radio_messages[std::to_underlying(tfx::flight::landing_radio::approach_speed)]);
    }
    if(state.mode == tfx::cockpit::aiming_mode::ground && ground_designation) state.ground_marker = projection.integer({designated_point.x >> 8,designated_point.y >> 8,designated_point.z >> 8});
    if(state.mode == tfx::cockpit::aiming_mode::navigation || state.cannon || weapon.category == tfx::weapons::bomb_impact_category) {
      vec3<int32_t> const origin{vec3<uint32_t>{player.integer_position} * static_cast<uint32_t>(tfx::actors::position_unit)};
      auto const anchor{tfx::math::place_anchor(origin,player.angles,tables.sight_mount(player.aircraft,armament.selected))};
      auto const project_prediction{[&](vec3<int32_t> const &position) {
        if(auto const point{projection.integer({position.x >> 8,position.y >> 8,position.z >> 8})}) return vec2<int16_t>{*point};
        return vec2<int16_t>{-1,-1};
      }};
      if(state.mode == tfx::cockpit::aiming_mode::navigation || state.cannon) {
        auto const samples{projectiles.predict_cannon(anchor,player.angles,player.fixed_speed,movement_height_bounds)};
        for(unsigned int index{0}; index != samples.size(); ++index) state.cannon_path[index] = project_prediction(samples[index]);
      }
      if(state.mode != tfx::cockpit::aiming_mode::navigation && weapon.category == tfx::weapons::bomb_impact_category) state.bomb_impact.emplace(project_prediction(projectiles.predict_bomb_impact(weapon_index,anchor,player.angles,player.fixed_speed,movement_height_bounds)));
    }
    auto const aiming_effects{[&] {
      tfx::cockpit::capture_hud capture{renderer.output,&hud_vectors};
      return aiming_frame->draw_forward(renderer,state);
    }()};
    hud_underlay=renderer.output.coverage;
    std::ranges::fill(renderer.output.coverage,0);
    if(!paused && aiming_effects.lock_acquired) {
      // Native 7FC50: formation leaders receive the two-sample announcement;
      // other aircraft and the cruise ground point use the urgent tracking call.
      bool const leader{seeker.guidance && air_simulation.aircraft()[*seeker.guidance] && air_simulation.aircraft()[*seeker.guidance]->flight.pose.leader == 255};
      if(leader && sound) sound->sequence(audio::sample_priority::normal,{std::to_underlying(audio::sample_id::tracking),std::to_underlying(audio::sample_id::looks_like)});
      else sample(audio::sample_priority::urgent,audio::sample_id::tracking);
    }
  }};
  auto const draw{[&]{
    if(!paused) paused_camera_update=false;
    scene.set_render_paused(paused);
    struct restore_scene { tfx::world::scene &value; ~restore_scene() { value.set_render_paused(false); } } restore{scene};
    auto const previous_pose{view_motion.last_pose()};
    auto const previous_view{views.value().current};
    render::preview_state sky_frame{sky_state,paused};
    render::preview_state lighting_frame{palette_lighting,paused};
    render::preview_state rain_frame{options.rain,paused};
    render::preview_state radio_frame{radio,paused};
    render::preview_state designator_frame{designator_display,paused};
    if(paused) recording_timestamp.reset();
    hud_vectors.begin(video.resolution);hud_underlay.clear();
    instrument_positions = {};
    if(!inspect) frame_pages.begin_frame(renderer.output);
    auto const prepare_lighting{[&](tfx::render::camera const &view) {
      auto const projected_view{view.projector()};
      if(!night_flak) lighting_frame->flash(tfx::render::glare_level(projected_view,{
        .camera_anchor{vec3<int32_t>{view.world_anchor}},
        .maximum{vec2<int16_t>{projected_view.maximum}},
        .view{views.value().current},
        .cloud_layer{environment.lighting.cloud_layer},
        .layer_y{environment.lighting.layer_y},
        .flak_flash{flak_flash},
      }));
      if(!paused) flak_flash = false;
      auto const request{tfx::world::advance_sky(sky_frame.value(),{
        .suppressed{environment.lighting.sun_suppressed},
        .cloud_layer{environment.lighting.cloud_layer},
        .storm{environment.lighting.storm},
        .animate{!paused && sky_animation},
        .layer_y{environment.lighting.layer_y},
        .player{vec3<int32_t>{player.integer_position}},
        .camera_anchor{vec3<int32_t>{view.world_anchor}},
      })};
      if(!paused) sky_animation = false;
      if(request && projected_view.sphere_visible(vec3<int32_t>{request->position},tfx::world::sky_visibility_radius)) {
        vec3<int32_t> const reference{static_cast<int32_t>(std::floor(view.position.x)),static_cast<int32_t>(std::floor(view.position.y)),static_cast<int32_t>(std::floor(view.position.z))};
        scene.queue_model(std::string{tfx::world::sky_model_name(request->model)},tfx::world::effect_position(request->position,reference));
      }
      if(tfx::world::consume_lightning_flash(sky_frame.value(),projected_view)) lighting_frame->flash(12);
      auto const lighting{lighting_frame->advance({
        .camera_y{view.world_anchor.y},
        .pitch{player.angles.x},
        .roll{player.angles.z},
        .view{views.value().current},
        .hud_green{static_cast<uint8_t>(hud_green)},
        .normal_changed{palette_selection.value().normal_changed},
        .blackout{g_effects.mode},
        .blackout_level{g_effects.blackout_level},
        .redout_level{g_effects.redout_level},
      })};
      if(lighting.changed) palette_selection.invalidate_normal();
      if(auto const upload{palette_selection.advance({
        .view{views.value().current},
        .projectiles_present{!projectiles.projectiles().empty()},
        .infrared_available{!environment.lighting.infrared_palette_name.empty()},
        .night{(environment.lighting.status & tfx::world::night_status) != 0},
        .night_vision{night_vision},
      })}) {
        frame_dac = *upload == tfx::render::palette_bank::infrared ? infrared_dac : lighting_frame->colours();
        if(*upload == tfx::render::palette_bank::normal) colours = tfx::render::expand_dac_palette(frame_dac);
        frame_colours = *upload == tfx::render::palette_bank::infrared ? &infrared_colours : &colours;
      }
    }};
    auto const paint_precipitation{[&](tfx::render::camera const &view) {
      if(inspect || result_evaluated) return;
      if(auto const count{tfx::render::precipitation_count({
        .enabled{preferences.effects.precipitation != 0},
        .storm{environment.lighting.storm},
        .layer_y{environment.lighting.layer_y},
        .player_y{player.position.y},
        .view{views.value().current},
        .speed{player.fixed_speed},
      })}) {
        auto const projection{view.projector()};
        auto const &wind{physical_player.wind.velocity_ned};
        rain_frame->paint(renderer.output,{
          .matrix{projection.matrix},
          .camera{tfx::math::signed_word(static_cast<uint16_t>(projection.position_q16.x >> 16)),tfx::math::signed_word(static_cast<uint16_t>(projection.position_q16.y >> 16)),tfx::math::signed_word(static_cast<uint16_t>(projection.position_q16.z >> 16))},
          .origin{vec2<int16_t>{projection.origin}},
          .maximum{vec2<int16_t>{projection.maximum}},
        },*count,{wind.y,wind.x});
      }
    }};
    auto const paint_scene{[&](tfx::render::camera const &view, vec3<int32_t> const &reference, bool const primary) {
      if(primary && ejection.value().stage == tfx::flight::ejection_stage::pilot) scene.queue_actor(aircraft_visual,vec3d{ejection.value().aircraft_position_q8} / tfx::actors::position_unit,ejection.value().aircraft_orientation.angles(),nullptr,tfx::world::submission_phase::before_sky);
      queue_projectiles(reference);
      scene.set_query_reference(inspect ? nullptr : &player.integer_position);
      if(!inspect) select_actor_draw_region(reference);
      options.projection_origin = vec2<int16_t>{view.projection_origin};
      std::function<void()> const effects{primary ? std::function<void()>{[&] { queue_effects(view); }} : std::function<void()>{}};
      auto const render_view{tfx::render::scene_camera(view,video)};
      scene.set_visual_lod_scale(video.lod_scale);
      tfx::render::paint_horizon(world_renderer.output,render_view,ground_phase.sample());
      world_renderer.clipping = tfx::render::raster_clip::pixels({0,0},vec2<int>{render_view.viewport_maximum});
      auto const rendered{scene.draw(world_renderer,render_view,scene_radius,effects,inspect ? std::nullopt : std::optional<int32_t>{reference.y},ground.height,primary ? tfx::world::scene_pass::primary : tfx::world::scene_pass::instrument)};
      if(primary && !paused) clean_trails();
      if(!paused && rendered.eject_player) ejection.request();
    }};
    // Native CODE 47C94 is one shared carried-model table. Visibility is
    // independent of quantity (for example an empty JP233 dispenser remains).
    std::array<tfx::models::shape const*,tfx::flight::store_slots> carried{};
    for(unsigned int slot{0}; slot != carried.size(); ++slot) {
      auto const &pylon{armament.pylons[slot]};
      if(pylon.visible && pylon.weapon != tfx::weapons::empty_pylon) carried[slot] = &assets.shape(tables.weapons.at(pylon.weapon).carried);
    }
    scene.set_carried_models(carried);
    int16_t const horizon_phase{ground_phase.sample()};
    if(!inspect && !paused && !result_evaluated) update_refuelling();
    int16_t const requested_offset_y{cockpit_position.value().y};
    if(!inspect && !designator_view) {
      if(!tables.instruments().viewports[view_index()].panel.empty()) cockpit_position.move_to(cockpit_frame.clamp(view_index(),cockpit_position.value()));
      views.offset(cockpit_position.value());
      auto context{camera_context()};
      if(paused) context.pan=0;
      if((!paused || paused_camera_update) && !view_motion.advance(views,context)) throw std::runtime_error{"unimplemented flight camera"};
      if(!paused) if(auto const movement{view_motion.last_pose().ground_motion}) ground_phase.advance(*movement);
      auto const entry{view_motion.last_pose().player_model_entry};
      scene.show_actor(player_visual,entry.has_value());
      if(entry) scene.actor_event(player_visual,*entry);
      bool const integer_model{view_motion.last_pose().model_source == tfx::cockpit::player_model_source::integer};
      auto const model_position{integer_model ? vec3d{context.position} : vec3d{context.position_q8} / tfx::actors::position_unit};
      scene.set_player_rendering(player_visual,model_position,context.angles,integer_model ? tfx::world::animation_policy::when_visible : tfx::world::animation_policy::frozen);
    } else {
      scene.show_actor(player_visual,false);
      scene.set_player_rendering(player_visual,vec3d{player.position},player.angles,tfx::world::animation_policy::frozen);
      if(!inspect && !paused && designator_view) {
        view_motion.advance_ground_reference(camera_context());
        if(!paused) if(auto const movement{view_motion.last_pose().ground_motion}) ground_phase.advance(*movement);
      }
    }
    if(!inspect && designator_view) {
      // Native 0x92144 leaves the installed view unchanged for viewport 19.
      // Its ordinary world pass (and C5BEC recording) precedes the 0x92c24
      // callback, which installs the new gimbal and paints a second world.
      if(!scene.last_camera()) throw std::runtime_error{"designator has no inherited world camera"};
      auto primary{*scene.last_camera()};
      // The gimbal replaces the projection cache, but not 134F0/134F8/13500.
      // The ordinary pass keeps the last normal view's terrain/lighting anchor.
      primary.world_anchor = vec3<int32_t>{view_motion.last_pose().anchor};
      primary.projection_origin = cockpit_frame.projection_origin(view_index(),cockpit_position.value());
      primary.viewport_minimum = {0,0};primary.viewport_maximum = {tfx::render::screen_width - 1,tfx::render::screen_height - 1};
      prepare_lighting(primary);
      if(!paused) advance_air_tracking(primary.projector());
      paint_scene(primary,primary.world_anchor,true);
      paint_precipitation(primary);
    }
    if(!inspect) update_designator(paused);
    if(!inspect) options.ground_reference = vec3<int32_t>{view_motion.value().ground_reference};
    auto const projection_origin{cockpit_frame.projection_origin(view_index(),cockpit_position.value())};
    auto view{inspect ? tfx::render::inspection_camera(position,heading,pitch) : designator_view ? tfx::cockpit::designator_pose(designator).camera(projection_origin) : view_motion.last_pose().camera(projection_origin)};
    if(!inspect && !designator_view) {
      external_orbit.select(views.value().current);
      view = external_orbit.apply(view,camera_context(),view_motion.value().last_missile);
    }
    if(!inspect) {
      auto const &layout{tables.instruments().viewports[view_index()]};
      view.viewport_maximum = {tfx::render::screen_width - 1,static_cast<int16_t>(layout.bottom + (layout.panel.empty() ? 0 : cockpit_position.value().y))};
      tfx::render::interface_layout const interface{video.resolution};
      auto const actual_view{tfx::render::scene_camera(view,video)};
      view.point_focal_distance = actual_view.focal_distance / interface.scale;
      trail_camera = actual_view; // MFD passes replace scene.last_camera() later.
      impact_projection.emplace(view.projector());
      if(!designator_view) prepare_lighting(view);
    }
    view.far_distance = scene_radius ? scene_radius * tfx::world::cell_units : std::numeric_limits<double>::infinity();
    if(inspect) renderer.background(view,0,32);
    else if(designator_view || view_motion.last_pose().horizon_angles) {
      auto const angles{designator_view ? vec2<tfx::math::angle>{static_cast<tfx::math::angle>(-designator.pitch),0} : vec2<tfx::math::angle>{*view_motion.last_pose().horizon_angles}};
      tfx::render::paint_horizon(renderer.output,{
        .pitch{angles.x},
        .roll{angles.y},
        .camera_y{designator_view ? tfx::math::signed_word(static_cast<uint16_t>(static_cast<int32_t>(std::floor(view.position.y)))) : view_motion.last_pose().horizon_camera_y},
        // Ordinary camera callbacks paint before 0x92969 advances the ground
        // reference. The designator's 0x8fcd4 call runs afterwards instead.
        .ground_phase{designator_view ? ground_phase.sample() : horizon_phase},
        .maximum{vec2<int16_t>{view.viewport_maximum}},
        .origin_y{static_cast<int16_t>(view.projection_origin.y)},
        .projection_shift{tfx::render::projection_shift(view.opening),tfx::render::projection_shift(view.opening)},
      });
    }
    // Native 0x93328 projects after 0x92144 installs this frame's actual
    // camera. External and target views must not use a forward-only matrix.
    if(!inspect && !paused && !designator_view) advance_air_tracking(*impact_projection);
    vec3<int32_t> const draw_reference{inspect ? vec3<int32_t>{view.position} : designator_view ? vec3<int32_t>{designator.camera} : vec3<int32_t>{view.world_anchor}};
    paint_scene(view,draw_reference,!designator_view);
    renderer.output.pixels.fill(0);
    renderer.output.track_writes();
    if(!designator_view) paint_precipitation(view);
    if(!inspect && result_evaluated) {
      // Diagnostic result display until the original presentation/menu graph supplies its result page.
      renderer.output.pixels.fill(0);
      renderer.text(90,35,"MISSION RESULTS",15);
      renderer.text(25,60,mission_progress.outcome == tfx::missions::session_outcome::aircraft_lost ? "AIRCRAFT LOST" : mission_progress.results.completed ? "OBJECTIVE COMPLETE" : "OBJECTIVE NOT COMPLETE",15);
      renderer.text(25,80,std::format("AIR KILLS {}",mission_progress.results.credited_air_kills),15);
      renderer.text(25,95,std::format("GROUND KILLS {}  TARGETS {}",mission_progress.results.credited_ground_kills,mission_progress.results.credited_targets),15);
      renderer.text(25,115,std::format("SCORE {}  RESULT {}",campaign["score"],campaign["miss_val"]),15);
      renderer.text(40,160,"ENTER OR ESCAPE TO CLOSE",15);
    } else if(!inspect) {
      auto const callback{tables.instruments().viewports[view_index()].callback_file};
      if(!designator_view && (callback == 0x806dc || callback == 0x80718 || callback == 0x80758)) draw_aiming(view,callback != 0x80758);
      bool const hide_external_information{!designator_view && callback == 0x80794 && !options.flight_information};
      // Several loss/ejection viewports have no display callback at all.
      // They neither draw nor advance the retained radio ticker.
      auto const radio_window{!callback || designator_view || callback == 0x80704 || hide_external_information ? std::optional<std::string>{} : radio_frame->advance(paused ? 0 : launch_elapsed,acceleration.active)};
      auto const paint_cockpit{[&] {
        frame_pages.refresh_panel(physical_player.powerplant.afterburner_control,requested_offset_y);
        cockpit_frame.paint(renderer.output,view_index(),cockpit_position.value(),nullptr,cockpit::panel_copy::refresh);
      }};
      if(!designator_view && view_index() == tfx::cockpit::forward_cockpit) {
        paint_cockpit();
        draw_instruments(radio_window);
      } else if(!designator_view && callback == 0x80704) paint_cockpit();
      else {
        if(!designator_view && callback == 0x80794 && options.flight_information) external_instruments.draw(renderer,{
          .height{player.position.y},
          .speed{player.fixed_speed},
          .heading{player.angles.y},
        });
        if(radio_window) instruments.draw_radio(renderer,*radio_window);
      }
      if(designator_view) designator_frame->draw(renderer,impact_projection->integer(designator.point).value_or(vec2<int16_t>{-1,-1}),designator_tracking_frame);
      if(flight_recorder.active()) {
        auto const &marker{tables.configuration_widgets().recording_marker};
        tables.presentation_fonts().draw_driver(renderer.output,{311,1},marker,0);
        tables.presentation_fonts().draw_driver(renderer.output,{310,0},marker,6);
      }
      if(!paused) engagement.finish_frame();
      launch_warning = projectiles.first_targeting_player();
      views.redraw(physical_player.powerplant.afterburner_control);
      if(designator_view) views.ready();
      if(views.value().ready) {
        views.consume();
        cockpit_position.move_to(views.value().offset);
        designator_view = views.value().current == tfx::cockpit::view_mode::designator;
      }
      physical_player.powerplant.afterburner_control = views.value().redraw;
    }
    if(paused_camera_update && views.value().read==views.value().written
       && previous_view==views.value().current
       && previous_pose.position_q16==view_motion.last_pose().position_q16
       && previous_pose.matrix==view_motion.last_pose().matrix) paused_camera_update=false;
    if(!inspect) frame_pages.finish_frame(renderer.output);
    display_frame = world_renderer.output;
    for(unsigned int slot{0}; slot != instrument_positions.size(); ++slot) if(instrument_positions[slot]) {
      auto const &position{*instrument_positions[slot]};
      auto const &image{instrument_renderers[slot].output};
      for(int row{0}; row != image.size.y; ++row) std::copy_n(image.pixels.begin() + row * image.size.x,image.size.x,display_frame.pixels.begin() + (position.y + row) * display_frame.size.x + position.x);
    }
    if(result_evaluated) display_frame = tfx::render::scale_image(renderer.output,video.resolution);
    else {
      hud_vectors.compose(display_frame,renderer.output.coverage);
      if(!hud_underlay.empty()) for(size_t i=0;i<hud_underlay.size();++i) renderer.output.coverage[i]|=hud_underlay[i];
      // Preserve the HUD above earlier precipitation; only later cockpit
      // writes were used as its occlusion mask.
      auto optical{std::move(display_frame.hud_coverage)};
      tfx::render::composite_interface(display_frame,renderer.output);
      display_frame.hud_coverage=std::move(optical);
    }
    if(flight_recorder.active() && !paused && !inspect && !result_evaluated) {
      double const now{glfwGetTime()};
      auto const duration{std::chrono::microseconds{std::max(int64_t{1},static_cast<int64_t>(std::llround((recording_timestamp ? now - *recording_timestamp : 4 / tfx::simulation::timer_hz) * 1000000)))}};
      try { flight_recorder.append(display_frame,palette_selection.value().infrared ? infrared_dac : lighting_frame->colours(),duration);recording_timestamp = now; }
      catch(std::exception const &error) {
        std::cerr << "Cannot record flight: " << error.what() << '\n';
        try { flight_recorder.finish(); } catch(std::exception const &closing) { std::cerr << "Cannot finish flight recording: " << closing.what() << '\n'; }
      }
    }
  }};
  // A diagnostic still/inspector can draw an unadvanced scene. Ordinary
  // flight first advances the simulation, then consumes camera/HUD/SSD state.
  if(inspect || (screenshot && !flight_frames)) draw();
  unsigned int complete_frames{0};
  for(; complete_frames != flight_frames && !result_evaluated; ++complete_frames) {
    // Match the real flight loop: drawing consumes/produces retained camera,
    // HUD and SSD state before mission/frame-tail decisions observe it.
    advance_world({},4,static_cast<double>(4 * .02f));
    draw();
    finish_world(4);
    projectiles.finish_frame();
    std::erase_if(trail_samplers,[&](auto const &entry) { return !projectiles.projectiles().contains(entry.first); });
  }
  if(flight_frames) std::cout << "Complete flight frames: " << complete_frames << std::endl;
  if(frontend_flight && result_evaluated) return flight_result::finished;
  std::cout << "TFX " << mission_name << " / " << environment.name << " / " << environment.lighting_name << ": " << scene.parts_drawn << " model parts, " << scene.primitives_drawn << " drawing primitives" << std::endl;
  if(!scene.warnings.empty()) {
    unsigned int printed{0};
    for(auto const &warning : scene.warnings) if(printed++ < 6) std::cerr << "Preview note: " << warning << std::endl;
    if(scene.warnings.size() > 6) std::cerr << "Preview notes: " << scene.warnings.size() << " distinct limitations; this is a flight-integration build" << std::endl;
  }
  if(screenshot) {
    display_frame.screenshot(*screenshot,*frame_colours);
    std::cout << "Scene screenshot: " << *screenshot << std::endl;
    return flight_result::finished;
  }
  std::unique_ptr<tfx::platform::window> owned_window;
  if(!shared_window) owned_window = std::make_unique<tfx::platform::window>(scale,video.resolution,video.automatic_resolution);
  auto &window{shared_window ? *shared_window : *owned_window};
  active_window = &window;
  // Wayland maps the window after its first buffer: present before waiting for focus.
  window.present(display_frame.colourise(*frame_colours),display_frame.size);
  auto *const host{window.get()};
  std::optional<int> joystick;
  tfx::flight::joystick_buttons stick_buttons;
  auto const poll_stick{[&]() -> uint8_t {
    if(preferences.control != tfx::flight::control_source::joystick || arcade_mode) return 0;
    if(!joystick || !glfwJoystickPresent(*joystick)) {
      joystick.reset();
      for(int device{options.joystick ? static_cast<int>(*options.joystick) : GLFW_JOYSTICK_1}; device <= (options.joystick ? static_cast<int>(*options.joystick) : GLFW_JOYSTICK_LAST); ++device) {
        int count{0};
        if(glfwJoystickPresent(device) && glfwGetJoystickAxes(device,&count) && count >= 2) { joystick = device;break; }
      }
    }
    if(!joystick) { simple_controls.source = tfx::flight::control_source::keyboard;simple_controls.joystick = {0,0};return 0; }
    simple_controls.source = tfx::flight::control_source::joystick;
    int count{0};
    auto const *axes{glfwGetJoystickAxes(*joystick,&count)};
    if(!axes || count < 2) return 0;
    simple_controls.joystick = tfx::flight::joystick_axes({axes[0],axes[1]},preferences.joystick_scale);
    auto const *buttons{glfwGetJoystickButtons(*joystick,&count)};
    uint8_t mask{0};
    for(int index{0}; buttons && index != std::min(count,4); ++index) if(buttons[index] == GLFW_PRESS) mask |= static_cast<uint8_t>(1u << index);
    return mask;
  }};
  // Arcade consumes relative mouse-style input; Simple has no mouse law.
  if((arcade_mode && simple_controls.source == tfx::flight::control_source::joystick) || (simple_mode && simple_controls.source == tfx::flight::control_source::mouse)) simple_controls.source = tfx::flight::control_source::keyboard;

  double previous_time{glfwGetTime()}, title_time{previous_time}, last_mouse_x{0}, last_mouse_y{0}, timer_fraction{0};
  unsigned int frames{0}, interval_frames{0};
  bool captured{false}, mouse_started{false}, store_trigger{false}, designator_mouse_active{false}, orbiting{false};
  std::array<bool,GLFW_KEY_LAST + 1> held{false};
  // A key that closed the launch screen is not a new in-flight press.
  for(int key{GLFW_KEY_SPACE}; key <= GLFW_KEY_LAST; ++key) held[static_cast<unsigned int>(key)] = glfwGetKey(host,key) == GLFW_PRESS;
  pause_input pause_keys;pause_keys.synchronise(held,window.focused());
  tfx::simulation::timer_counter counter{0};
  tfx::simulation::flight_clock flight_clock{counter};
  flight_clock.begin({
    .model{preferences.model},
  });
  double pitch_input{0}, roll_input{0};
  auto const handle_ejection{[&](bool const confirmed) {
    auto const result{ejection.escape(player.integer_position,player.angles,player.fixed_speed,confirmed)};
    if(result.disable_refuelling) refuelling.disengage();
    if(result.fixed_camera) views.fixed_position(*result.fixed_camera);
    for(auto const &event : result.events) {
      if(event.kind == tfx::flight::ejection_event_kind::player_model) scene.actor_event(player_visual,event.argument);
      else if(event.kind == tfx::flight::ejection_event_kind::camera) {
        views.enqueue(static_cast<tfx::cockpit::view_mode>(event.argument),player.fixed_speed);
      }
      else if(event.kind == tfx::flight::ejection_event_kind::sample && sound) sound->sequence(audio::sample_priority::normal,{event.argument,event.argument});
      else if(event.kind == tfx::flight::ejection_event_kind::music_cue && sound) sound->music_cue(static_cast<uint8_t>(event.argument));
    }
    if(result.end_flight) {
      mission_progress.ended = true;
    }
  }};
  window.take_scroll();
  while(window.poll()) {
    if(video.automatic_resolution && video.resolution != window.resolution()) {
      video.resolution = vec2<int>{window.resolution()};
      world_renderer.output.resize(video.resolution);
      display_frame.resize(video.resolution);
    }
    double const now{glfwGetTime()}, real_elapsed{now - previous_time}, elapsed{std::clamp(real_elapsed,0.0,.1)};
    previous_time = now;
    auto const wheel{window.take_scroll()};
    bool const focused{window.focused()};
    if(!focused) {
      captured = false;
      mouse_started = false;
      held.fill(false);
      store_trigger = false;
      pitch_input = 0;
      roll_input = 0;
      keyboard = {};
      simple_controls.keys = {};
      simple_controls.joystick = {0,0};
      stick_buttons = {};
      mouse_control = {};
      designator_mouse.reset();
    }
    pause_input::snapshot keys{};
    if(focused) for(int key=GLFW_KEY_SPACE;key<=GLFW_KEY_LAST;++key) keys[static_cast<size_t>(key)]=glfwGetKey(host,key)==GLFW_PRESS;
    if(pause_keys.update(keys,paused && !inspect && !result_evaluated,focused)) paused=false;
    auto const down{[&](int const key){ return keys[static_cast<size_t>(key)] && !pause_keys.consumed(key); }};
    auto const pressed{[&](int const key){
      bool const active{down(key)}, edge{active && !held[static_cast<unsigned int>(key)]};
      held[static_cast<unsigned int>(key)] = active;
      if(!edge) return false;
      using tfx::flight::cheat_key;
      auto const letter{key == GLFW_KEY_P ? cheat_key::p : key == GLFW_KEY_L ? cheat_key::l : key == GLFW_KEY_O ? cheat_key::o : cheat_key::other};
      bool const before{options.cheat.value().enabled};
      bool const consumed{options.cheat.press(letter,down(GLFW_KEY_RIGHT_SHIFT))};
      if(before != options.cheat.value().enabled) radio.show(tables.instruments().cheat_messages[options.cheat.value().enabled]);
      // Native 83970 clicks before dispatching a bound make code; the PLOP
      // prepass clears consumed cheat letters before they reach that handler.
      if(!consumed && !inspect && !result_evaluated) sample(audio::sample_priority::background,audio::sample_id::key_click);
      return !consumed;
    }};
    bool const escape_key{pressed(GLFW_KEY_ESCAPE)};
    bool const control_key{down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)};
    if((inspect || result_evaluated) && escape_key) break;
    bool const quit_key{pressed(GLFW_KEY_Q)};
    bool const quit_prompt{(down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) && quit_key};
    if((inspect || result_evaluated) && quit_prompt) break;
    if(result_evaluated && (pressed(GLFW_KEY_ENTER) || pressed(GLFW_KEY_KP_ENTER))) break;
    if(!inspect && pressed(GLFW_KEY_4)) {
      try {
        auto const path{save_screenshot(options.data_directory,display_frame,frame_dac)};
        std::cout << "Screenshot: " << path << '\n';
      } catch(std::exception const &error) {
        std::cerr << "Cannot save screenshot: " << error.what() << '\n';
      }
    }
    bool const map_key{pressed(GLFW_KEY_M)};
    bool const map_prompt{map_key && !down(GLFW_KEY_LEFT_SHIFT) && !down(GLFW_KEY_RIGHT_SHIFT)};
    bool const information_key{pressed(GLFW_KEY_D)};
    bool const information_shift{down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)};
    bool const rearm_key{information_key && (down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)) && options.cheat.value().enabled && !player.collision && !player_disabled && !ejected()};
    bool const rearm_prompt{rearm_key && !mission_setup.default_weapons && !paused};
    if(!inspect && !result_evaluated && map_key && !map_prompt && mission_setup.completion && mission_setup.completion->text) radio.show(*mission_setup.completion->text);
    if(!inspect && !result_evaluated && (map_prompt || rearm_prompt || quit_prompt || (escape_key && !control_key) || pressed(GLFW_KEY_GRAVE_ACCENT))) {
      captured = mouse_started = false;
      preferences.hud_green = static_cast<uint16_t>(hud_green);
      auto action{tfx::presentation::configuration_action::resume};
      if(map_prompt) {
        std::array<std::vector<tfx::cockpit::tactical_map_aircraft>,2> aircraft;
        std::array<std::vector<vec3<int32_t>>,2> surface;
        for(unsigned int index{0}; index != air_simulation.aircraft().size(); ++index) {
          auto const &actor{air_simulation.aircraft()[index]};
          if(!actor || (actor->flight.pose.flags & std::to_underlying(tfx::actors::air_flag::retired))) continue;
          auto const &pose{actor->flight.pose};
          auto const profile{std::ranges::find(profiles,pose.profile_id,&tfx::actors::profile::id)};
          unsigned int const side{index / tfx::actors::aircraft_per_side};
          aircraft[side].push_back({
            .reading{
              .name{profile->name},
              .position_q8{vec3<int32_t>{pose.position}},
              .heading{pose.angles.y},
              .speed_q16{pose.speed},
            },
            .selected{radar.selected == index},
          });
        }
        // Original enumerators visit each pool in descending slot order.
        for(auto &pool : aircraft) std::ranges::reverse(pool);
        for(auto const &actor : surface_actors | std::views::reverse) {
          auto const &pose{actor.state.pose};
          if(!visible_surface_contact(actor)) continue;
          surface[std::to_underlying(pose.side)].emplace_back(pose.position);
        }
        tfx::cockpit::tactical_map_snapshot state{
          .player{vec3<int32_t>{player.integer_position}},
          .heading{player.angles.y},
          .route{player_route},
          .targets{map_targets},
          .landing_cell{landing.value().enabled ? std::optional{vec2<int16_t>{landing.value().cell}} : std::nullopt},
          .hostile_aircraft{aircraft[0]},
          .allied_aircraft{aircraft[1]},
          .hostile_surface{surface[0]},
          .allied_surface{surface[1]},
        };
        // Original 0x8700e uploads the current normal DAC bank, including
        // changes made while infrared left normal uploads pending.
        auto const map_colours{tfx::render::expand_dac_palette(palette_lighting.colours())};
        show_tactical_map(window,tactical_map,state,map_colours);
      } else if(rearm_prompt) {
        glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        if(arm_mission(window,game,tables,armament,player.aircraft,mission_setup.weapons,tfx::presentation::arming_entry::rearming) == arming_result::closed) break;
        auto const &last_thumbnail{tables.arming_art().rows.back()};
        options.projection_origin = (last_thumbnail.minimum + last_thumbnail.maximum) / int16_t{2};
        reset_weapon_targets();
        // Native 0x85d1d still performs D/Shift+D after the arming call.
        finish_information_key(information_shift);
      } else action = configure_flight(window,tables,preferences,options.recording_name,options.data_directory,renderer.output,*frame_colours,sound,quit_prompt);
      if(action == tfx::presentation::configuration_action::enrol) return flight_result::enrol;
      if(action == tfx::presentation::configuration_action::quit) break;
      if(action == tfx::presentation::configuration_action::eject) { handle_ejection(true);paused = false; }
      if(!map_prompt && !rearm_prompt) {
        simple_mode = preferences.model == tfx::simulation::flight_model::simple;
        arcade_mode = preferences.model == tfx::simulation::flight_model::arcade;
        player_flight.select_model(preferences.model);
        air_world.model = preferences.model;
        simple_controls.source = preferences.control;
        player.crashing = preferences.effects.crashing != 0;
        environment.lighting.status = tfx::world::apply_detail_status(environment.lighting.status,preferences.detail);
        if(preferences.effects.g_force == tfx::configuration::g_effects::none) { g_effects = {};palette_lighting.flash(tfx::render::lighting_stages); }
      }
      // Arming saves/restores the displayed DAC bank. Forcing another normal
      // blend here would accumulate fixed-palette rounding after every rearm.
      if(!rearm_prompt) palette_selection.invalidate_normal();
      recording_timestamp.reset();
      mouse_control = {};
      pitch_input = roll_input = 0;
      store_trigger = false;
      designator_mouse.reset();
      window.take_scroll();
      // The modal screen consumed its closing make-code. Do not turn a
      // still-held M/Escape into another map request or an ejection press.
      for(int key{GLFW_KEY_SPACE}; key <= GLFW_KEY_LAST; ++key) {
        keys[static_cast<size_t>(key)]=window.focused() && glfwGetKey(host,key)==GLFW_PRESS;
        held[static_cast<size_t>(key)]=keys[static_cast<size_t>(key)];
      }
      pause_keys.synchronise(keys,window.focused());
      flight_clock.resume(counter,{
        .model{preferences.model},
      });
      previous_time = title_time = glfwGetTime();
      interval_frames = 0;
      continue;
    }
    if(!inspect && !result_evaluated) {
      bool const pause_letter{pressed(GLFW_KEY_P)}, pause_key{pressed(GLFW_KEY_PAUSE)};
      if(pause_letter || pause_key) paused = !paused;
    }
    auto const context{camera_context()};
    tfx::cockpit::view_key_context const view_keys{
      .crashed{player.collision != 0},
      .ejected{player_disabled || ejected()},
      .landing{automatic_landing.phase != tfx::flight::automatic_landing_phase::inactive},
      .shift{down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)},
      .speed{player.fixed_speed},
      .position{vec3<int32_t>{player.integer_position}},
      .angles{vec3<tfx::math::angle>{player.angles}},
      .height_bounds{vec2<int32_t>{movement_height_bounds}},
      .missile_available{context.missile.has_value()},
      .target_available{context.observed_target_q8.has_value()},
      .target_is_ground{ground_designation.has_value() && !seeker.guidance && !context.impact_target_q8},
      .tracking_available{context.tracking_target_q8.has_value()},
    };
    std::array<std::pair<int,tfx::cockpit::view_key>,10> constexpr view_bindings{{
      {GLFW_KEY_F1,tfx::cockpit::view_key::front},
      {GLFW_KEY_F2,tfx::cockpit::view_key::left},
      {GLFW_KEY_F3,tfx::cockpit::view_key::right},
      {GLFW_KEY_F4,tfx::cockpit::view_key::rear},
      {GLFW_KEY_F5,tfx::cockpit::view_key::chase},
      {GLFW_KEY_F6,tfx::cockpit::view_key::fly_past},
      {GLFW_KEY_F7,tfx::cockpit::view_key::tactical},
      {GLFW_KEY_F8,tfx::cockpit::view_key::missile},
      {GLFW_KEY_F9,tfx::cockpit::view_key::target},
      {GLFW_KEY_F10,tfx::cockpit::view_key::virtual_cockpit},
    }};
    views.offset(cockpit_position.value());
    auto const select_view{[&](tfx::cockpit::view_key const action) {
      if(paused) paused_camera_update=true;
      external_orbit.reset();
      mouse_started = false;
      pitch_input = roll_input = 0;
      mouse_control = {};
      auto const events{views.press(action,view_keys)};
      if(!paused && events.fly_past_sound && (player.left_engine || player.right_engine)) {
        sample(audio::sample_priority::normal,audio::sample_id::fly_past);
        lapc_effect(audio::lapc_effect_cue::fly_past);
      }
      cockpit_position.move_to(views.value().offset);
    }};
    if(!inspect && !result_evaluated) for(auto const &[key,action] : view_bindings) if(pressed(key)) select_view(action);
    uint8_t const stick_mask{focused && !inspect && !paused && !result_evaluated ? poll_stick() : uint8_t{0}};
    bool const designator_mouse_mode{!inspect && !result_evaluated && designator_view};
    if(designator_mouse_mode != designator_mouse_active) {
      designator_mouse_active = designator_mouse_mode;
      mouse_started = false;
      pitch_input = roll_input = 0;
      mouse_control = {};
      designator_mouse.reset();
    }
    bool const orbit_input{!inspect && !designator_view && external_orbit.select(views.value().current)};
    if(orbit_input != orbiting) {
      orbiting = orbit_input;
      mouse_started = false;
      pitch_input = roll_input = 0;
      mouse_control = {};
    }
    bool const capture{focused && (inspect ? glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS : (orbiting || designator_mouse_mode || (!paused && (arcade_mode || simple_controls.source == tfx::flight::control_source::mouse))) && !result_evaluated)};
    if(focused && !result_evaluated) {
      if(designator_mouse_mode) designator_mouse.scroll(wheel.y);
      else if(orbiting) external_orbit.scroll(wheel.y);
    }
    bool const capture_changed{capture != captured};
    if(capture_changed) {
      captured = capture;
      mouse_started = false;
      glfwSetInputMode(host,GLFW_CURSOR,captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    // Capture can enqueue a cursor warp. Let the next poll consume it before
    // taking a new baseline, otherwise that warp becomes a steering command.
    if(captured && !capture_changed) {
      double mouse_x{0}, mouse_y{0};
      glfwGetCursorPos(host,&mouse_x,&mouse_y);
      if(mouse_started) {
        if(inspect) { heading += (mouse_x - last_mouse_x) * .004; pitch -= (mouse_y - last_mouse_y) * .004; }
        else if(designator_mouse_mode) designator_mouse.move({mouse_x - last_mouse_x,mouse_y - last_mouse_y});
        else if(orbiting) external_orbit.move({mouse_x - last_mouse_x,mouse_y - last_mouse_y});
        else { pitch_input += mouse_y - last_mouse_y; roll_input += mouse_x - last_mouse_x; }
      }
      last_mouse_x = mouse_x;
      last_mouse_y = mouse_y;
      mouse_started = true;
    }
    std::optional<tfx::simulation::timer_ticks> frame_ticks;
    if(inspect) {
      heading += (down(GLFW_KEY_RIGHT) - down(GLFW_KEY_LEFT)) * elapsed;
      pitch += (down(GLFW_KEY_UP) - down(GLFW_KEY_DOWN)) * elapsed;
      pitch = std::clamp(pitch,-1.5,1.5);
      auto const camera{tfx::render::inspection_camera(position,heading,pitch)};
      double const movement{1500 * elapsed * (down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT) ? 6 : 1)};
      position += camera.forward * ((down(GLFW_KEY_W) - down(GLFW_KEY_S)) * movement);
      position += camera.right * ((down(GLFW_KEY_D) - down(GLFW_KEY_A)) * movement);
      position.y += (down(GLFW_KEY_F) - down(GLFW_KEY_R)) * movement;
      timer_fraction += real_elapsed * tfx::world::timer_hz;
      auto const step{static_cast<uint64_t>(std::floor(timer_fraction))};
      timer_fraction -= static_cast<double>(step);
      scene.advance(static_cast<tfx::world::timer_ticks>(step));
    } else if(!paused && !result_evaluated) {
      if(pressed(GLFW_KEY_V) && (down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT))) {
        try {
          if(flight_recorder.active()) flight_recorder.finish();
          else flight_recorder.begin(options.data_directory / options.recording_name);
          recording_timestamp.reset();
        } catch(std::exception const &error) { std::cerr << "Cannot change flight recording: " << error.what() << '\n'; }
      }
      if(pressed(GLFW_KEY_T)) {
        if(acceleration.toggle()) radio.show(tables.instruments().time_warp_message,true);
        else radio.stop();
      }
      if(escape_key && control_key) handle_ejection(false);
      std::array<std::pair<int,tfx::flight::defensive_key>,6> constexpr defensive_bindings{{
        {GLFW_KEY_E,tfx::flight::defensive_key::ecm},
        {GLFW_KEY_S,tfx::flight::defensive_key::stealth},
        {GLFW_KEY_INSERT,tfx::flight::defensive_key::chaff},
        {GLFW_KEY_KP_0,tfx::flight::defensive_key::chaff},
        {GLFW_KEY_DELETE,tfx::flight::defensive_key::flare},
        {GLFW_KEY_KP_DECIMAL,tfx::flight::defensive_key::flare},
      }};
      for(auto const &[key,action] : defensive_bindings) if(pressed(key)) {
        auto const effects{tfx::flight::press_defensive_key(defences,action,{
          .aircraft{player.aircraft},
          .ejecting{ejected()},
          .collision{player.collision != 0},
          .disabled{player_disabled},
          .afterburner{player.afterburner},
        })};
        if(effects.clear_targeting) { radar.selected.reset();seeker.tracking.reset();seeker.guidance.reset(); }
        if(effects.release) {
          tfx::weapons::actor_identity const owner{
            .pool{tfx::weapons::actor_pool::player},
          };
          projectiles.countermeasure(owner,actor_position(owner),effects.release->radar,effects.release->strength,air_simulation.random_stream());
        }
      }
      if(!ejected()) {
        tfx::flight::capture_shared_player(physical_player,simple_player,player_damage,simple_body);
        std::array<std::pair<int,tfx::flight::aerodynamic_pilot_key>,7> constexpr pilot_bindings{{
          {GLFW_KEY_6,tfx::flight::aerodynamic_pilot_key::cruise_speed},
          {GLFW_KEY_5,tfx::flight::aerodynamic_pilot_key::stick_pressure},
          {GLFW_KEY_7,tfx::flight::aerodynamic_pilot_key::waypoint},
          {GLFW_KEY_8,tfx::flight::aerodynamic_pilot_key::heading},
          {GLFW_KEY_9,tfx::flight::aerodynamic_pilot_key::track},
          {GLFW_KEY_0,tfx::flight::aerodynamic_pilot_key::autothrottle},
          {GLFW_KEY_A,tfx::flight::aerodynamic_pilot_key::toggle_autopilot},
        }};
        for(auto const &[key,action] : pilot_bindings) if(pressed(key)) {
          auto const events{tfx::flight::press_aerodynamic_pilot_key(physical_player,player_damage,stick_pressure,action,{
            .ground_elevation_steps{ground.height},
            .terrain_present{ground.present},
            .ejected{player_disabled},
            .recovery_available{preferences.effects.recovery != 0},
          })};
          if(events.notice) radio.show(tables.instruments().pilot_notices[std::to_underlying(*events.notice)]);
          if(events.external_status == tfx::flight::autopilot_external_status::on) {
            // Native 860CA selects the left controls MFD on engagement.
            uint16_t constexpr controls_page{8};
            display_selection.left = controls_page;
          }
          if(events.recovery.change == flight::aerodynamic_recovery_change::engaged) sample(audio::sample_priority::urgent,audio::sample_id::recovery);
          for(auto const event : events.recovery.afterburner) {
            reheat_sound(event);
            if(event == tfx::flight::afterburner_change::started) scene.actor_event(player_visual,9);
            else if(event == tfx::flight::afterburner_change::stopped) scene.actor_event(player_visual,10);
          }
          tfx::flight::publish_shared_player(simple_player,physical_player);
        }
        if(pressed(GLFW_KEY_TAB)) {
          auto &condition{player_damage.systems[std::to_underlying(tfx::flight::aircraft_system::air_brake)]};
          auto const events{tfx::flight::toggle_aerodynamic_recovery(physical_player.autopilot.recovery,physical_player.manual.controls,physical_player.powerplant,condition,{
            .afterburner_stages{physical_player.aircraft.powerplant.afterburner_stages},
            .available{preferences.effects.recovery != 0},
            .crashed{player.collision != 0},
            .ejected{player_disabled},
            .landed{player.landed},
          })};
          if(events.change == flight::aerodynamic_recovery_change::engaged) sample(audio::sample_priority::urgent,audio::sample_id::recovery);
          for(auto const event : events.afterburner) {
            reheat_sound(event);
            if(event == tfx::flight::afterburner_change::started) scene.actor_event(player_visual,9);
            else if(event == tfx::flight::afterburner_change::stopped) scene.actor_event(player_visual,10);
          }
          tfx::flight::publish_shared_player(simple_player,physical_player);
        }
        if(pressed(GLFW_KEY_ENTER) || pressed(GLFW_KEY_KP_ENTER)) select_weapon(tfx::weapons::weapon_role::air);
        if(pressed(GLFW_KEY_BACKSPACE) || pressed(GLFW_KEY_O)) select_weapon(tfx::weapons::weapon_role::ground);
        bool const navigation_key{pressed(GLFW_KEY_F)};
        bool const left_control{pressed(GLFW_KEY_LEFT_CONTROL)}, right_control{pressed(GLFW_KEY_RIGHT_CONTROL)};
        if((navigation_key || left_control || right_control) && !player.collision && !player_disabled) {
          sample(audio::sample_priority::normal,audio::sample_id::control_selection);
          armament.guns = true;cannon_navigation = true;
          physical_player.configure_controls(tfx::flight::input_configuration::cruise);
        }
        if(information_key && !player.collision && !player_disabled && !ejected()) {
          // DEFAULT_WEAPONS missions take the original immediate default-fit
          // branch; there is no arming dialog or paused-frame handoff.
          if(rearm_key) { armament.fit_default(mission_setup.weapons);reset_weapon_targets(); }
          finish_information_key(information_shift);
        }
        if(pressed(GLFW_KEY_J) && (down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT)) && !player.collision && !player_disabled) {
          tfx::flight::capture_shared_player(physical_player,simple_player,player_damage,simple_body);
          tfx::weapons::target_reference target{
            .untargeted{!seeker.tracking},
          };
          if(seeker.tracking) target.actor = tfx::weapons::actor_identity{
            .pool{tfx::weapons::actor_pool::aircraft},
            .index{*seeker.tracking},
          };
          tfx::weapons::selection_change change;
          armament.jettison({
            .position{vec3<int32_t>{player.integer_position}},
            .angles{vec3<tfx::math::angle>{player.angles}},
            .speed{player.fixed_speed},
            .target{target},
            .unlimited{unlimited_stores},
          },tables.store_mounts[std::to_underlying(player.aircraft) - 1],change,emit_store);
          apply_selection(change);
          auto &powerplant{physical_player.powerplant};
          if(powerplant.fuel > powerplant.fuel_capacity) powerplant.fuel = powerplant.fuel_capacity;
          tfx::flight::publish_shared_player(simple_player,physical_player);
        }
        bool const replay_key{pressed(GLFW_KEY_L)};
        bool const auto_land_key{replay_key && (down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT))};
        if(pressed(GLFW_KEY_I) || pressed(GLFW_KEY_BACKSLASH) || auto_land_key) {
          std::vector<tfx::flight::landing_surface> carriers;
          // Native surface enumeration is descending by slot and allied-only.
          for(auto const &actor : surface_actors | std::views::reverse) {
            auto const &pose{actor.state.pose};
            if(pose.side != tfx::actors::allegiance::allied || (pose.flags & (std::to_underlying(tfx::actors::surface_flag::destroyed) | std::to_underlying(tfx::actors::surface_flag::retired)))) continue;
            if(auto const port{scene.actor_landing_port(actor.visual)}) carriers.push_back({
              .position_q8{vec3<int32_t>{pose.position}},
              .port{*port},
              .destroyed{scene.actor_destroyed(actor.visual)},
            });
          }
          tfx::flight::landing_selection result;
          if(auto_land_key) {
            if(!player.collision && !player_disabled && !player.landed && automatic_landing.phase == tfx::flight::automatic_landing_phase::inactive) {
              if(player_damage.condition(tfx::flight::aircraft_system::landing_gear) == tfx::flight::system_condition::failed) radio.show(tables.instruments().automatic_landing_gear_failure);
              else {
                result = landing.select_runway(actor_program.player_route,[&](auto const &cell) { return scene.terrain_landing_port(cell); },carriers);
                if(landing.value().enabled) {
                  // Native 8428E calls the actual F5 handler, including its
                  // cockpit departure sequence, before enabling the approach.
                  views.press(tfx::cockpit::view_key::chase,{
                    .speed{player.fixed_speed},
                  });
                  automatic_landing.phase = tfx::flight::automatic_landing_phase::prepare;
                  result.navigation = true;
                  result.controls.emplace(tfx::flight::landing_controls{
                    .configuration{1},
                    .response{player.aircraft == tfx::flight::player_aircraft::nighthawk ? 1.0 : .5},
                    .pitch_response{preferences.model == simulation::flight_model::realistic ? .12 : .15},
                    .roll_response{preferences.model == simulation::flight_model::realistic ? .152 : .19},
                    .yaw_response{preferences.model == simulation::flight_model::realistic ? .024 : .03},
                  });
                }
              }
            }
          } else result = landing.toggle({
            .aircraft{player.aircraft},
            .crashed{player.collision != 0},
            .ejected{player_disabled || ejected()},
            .damaged{player_damage.accumulated != 0},
            .realistic{preferences.model == simulation::flight_model::realistic},
          },actor_program.player_route,[&](auto const &cell) { return scene.terrain_landing_port(cell); },carriers);
          if(result.afterburner_stop_sample) sample(audio::sample_priority::normal,audio::sample_id::afterburner_stop);
          if(result.music_cue && sound) sound->music_cue(static_cast<uint8_t>(*result.music_cue));
          if(result.navigation) { armament.guns = true;cannon_navigation = true; }
          for(auto const message : result.messages) radio.show(tables.instruments().landing_radio_messages[std::to_underlying(message)]);
          if(result.waypoint) physical_player.autopilot.navigation.route_index = static_cast<int16_t>(*result.waypoint + 1);
          if(result.controls) {
            physical_player.autopilot.recovery.configuration = result.controls->configuration;
            physical_player.input_gains.joystick = result.controls->response;
            physical_player.input_gains.pitch = result.controls->pitch_response;
            physical_player.input_gains.roll = result.controls->roll_response;
            physical_player.input_gains.rudder = result.controls->yaw_response;
          }
        }
        if(pressed(GLFW_KEY_APOSTROPHE)) {
          auto const result{refuelling.toggle(static_cast<uint8_t>(!armament.guns),false,player.collision != 0,player_disabled)};
          armament.guns = result.weapon_mode == 0;
          refuel_events(result.events);
        }
        if(replay_key && !down(GLFW_KEY_LEFT_ALT) && !down(GLFW_KEY_RIGHT_ALT) && !player.collision && !player_disabled) radio.replay();
        if(pressed(GLFW_KEY_C)) cycle_target();
        if(pressed(GLFW_KEY_1)) display_selection.cycle_left(!mission_setup.targets.empty());
        if(pressed(GLFW_KEY_2)) display_selection.cycle_centre(player.aircraft);
        if(pressed(GLFW_KEY_3)) {
          auto const *weapon{selected_weapon()};
          display_selection.cycle_right(player.aircraft,weapon ? weapon->category : tables.weapons[0].category);
        }
        if(pressed(GLFW_KEY_R)) display_selection.cycle_range();
        if(pressed(GLFW_KEY_N)) {
          if(down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) {
            if(!player.collision && !player_disabled) night_vision = !night_vision;
          } else if(!mission_setup.targets.empty()) ground_target_page = (ground_target_page + 1) % static_cast<unsigned int>(mission_setup.targets.size());
        }
        bool const designate_key{pressed(GLFW_KEY_Z)}, direct_designator_key{pressed(GLFW_KEY_WORLD_1)};
        if((designate_key || direct_designator_key) && !player.collision && !player_disabled) {
          if(designate_key && designator_view) designate_trigger = true;
          else if(auto const *weapon{selected_weapon()}; weapon && weapon->category == 3) {
            views.enqueue(tfx::cockpit::view_mode::designator,player.fixed_speed);
            designator.zoom = 0;
          }
        }
      }
      // Native 0x84430 has no aircraft-state guard and leaves the seeker
      // reticle, projected point and lock timers for their normal controller.
      if(pressed(GLFW_KEY_X)) { ground_designation.reset();mobile_designation.reset();radar.selected.reset();radar.cursor = -1; }
      tfx::flight::direction_keys const held_directions{
        .up{down(GLFW_KEY_UP) || down(GLFW_KEY_KP_8)},
        .left{down(GLFW_KEY_LEFT) || down(GLFW_KEY_KP_4)},
        .down{down(GLFW_KEY_DOWN) || down(GLFW_KEY_KP_2)},
        .right{down(GLFW_KEY_RIGHT) || down(GLFW_KEY_KP_6)},
      };
      if(!designator_view) tfx::flight::poll_keyboard(keyboard,{
        .directions{held_directions},
        .home{down(GLFW_KEY_HOME) || down(GLFW_KEY_KP_7)},
        .end{down(GLFW_KEY_END) || down(GLFW_KEY_KP_1)},
        .page_up{down(GLFW_KEY_PAGE_UP) || down(GLFW_KEY_KP_9)},
        .page_down{down(GLFW_KEY_PAGE_DOWN) || down(GLFW_KEY_KP_3)},
        .accelerate{down(GLFW_KEY_EQUAL) || down(GLFW_KEY_KP_ADD)},
        .decelerate{down(GLFW_KEY_MINUS) || down(GLFW_KEY_KP_SUBTRACT)},
        .o{down(GLFW_KEY_O)},
        .right_shift{down(GLFW_KEY_RIGHT_SHIFT)},
        .control{down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)},
        .automatic_landing{automatic_landing.phase != tfx::flight::automatic_landing_phase::inactive},
      });
      // Native adjacent bytes 0x1358a/b pack right-Shift orbit and Ctrl
      // translation into the low/high bytes of the camera's pan word.
      view_pan = static_cast<uint16_t>(keyboard.view_pan | static_cast<unsigned int>(keyboard.target_pan) << 8);
      if(pressed(GLFW_KEY_COMMA) && designator_view && designator.zoom) --designator.zoom;
      if(pressed(GLFW_KEY_PERIOD) && designator_view && designator.zoom < 10) ++designator.zoom;
      designator_input = {
        .up{held_directions.up},
        .down{held_directions.down},
        .left{held_directions.left},
        .right{held_directions.right},
      };
      if(pressed(GLFW_KEY_SPACE)) store_trigger = true;
      if(pressed(GLFW_KEY_G)) {
        auto const request{tfx::flight::press_gear_key({
          .aircraft{player.aircraft},
          .speed{player.fixed_speed},
          .gear_down{physical_player.gear_fraction != 0},
          .condition{player_damage.condition(tfx::flight::aircraft_system::landing_gear)},
          .inhibited{refuelling.value().phase != 0},
          .crashed{player.collision != 0},
          .disabled{player_disabled || ejected()},
          .automatic_landing{automatic_landing.phase != tfx::flight::automatic_landing_phase::inactive},
          .landed{player.landed},
        })};
        if(request.speed_warning) radio.show(tables.instruments().gear_speed_warning);
        if(request.gear_down) {
          sample(audio::sample_priority::normal,audio::sample_id::gear);
          lapc_effect(audio::lapc_effect_cue::mechanical_switch);
          player.gear_down = *request.gear_down;
          physical_player.gear_fraction = player.gear_down ? 1 : 0;
          player_damage.systems[std::to_underlying(tfx::flight::aircraft_system::landing_gear)] = player.gear_down ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal;
          scene.actor_event(player_visual,player.gear_down ? 3u : 4u);
        }
      }
      if(!player.collision && !player_disabled && !ejected()) {
        bool const raise_cockpit{pressed(GLFW_KEY_PAGE_UP)}, raise_keypad{pressed(GLFW_KEY_KP_9)};
        bool const lower_cockpit{pressed(GLFW_KEY_PAGE_DOWN)}, lower_keypad{pressed(GLFW_KEY_KP_3)};
        if(raise_cockpit || raise_keypad) cockpit_position.raise();
        if(lower_cockpit || lower_keypad) cockpit_position.lower();
        auto const engine_key{[&](flight::engine_side const side) {
          auto const index{static_cast<unsigned int>(side)};
          auto const system{side == flight::engine_side::left ? flight::aircraft_system::left_engine : flight::aircraft_system::right_engine};
          auto const other{side == flight::engine_side::left ? flight::aircraft_system::right_engine : flight::aircraft_system::left_engine};
          auto const request{flight::press_engine_key({
            .side{side},
            .condition{player_damage.condition(system)},
            .other_condition{player_damage.condition(other)},
            .afterburner{player.afterburner != 0},
            .inhibited{refuelling.value().phase != 0},
            .crashed{player.collision != 0},
            .disabled{player_disabled || ejected()},
          })};
          for(auto const event : request.ordered) switch(event) {
          case flight::engine_key_event::stop_afterburner:
            physical_player.powerplant.stop_afterburner();
            player.afterburner = 0;
            reheat_sound(flight::afterburner_change::stopped);
            scene.actor_event(player_visual,10);
            break;
          case flight::engine_key_event::stop_sample: sample(audio::sample_priority::urgent,audio::sample_id::engine_stop);break;
          case flight::engine_key_event::start_samples:
            if(sound) sound->sequence(audio::sample_priority::normal,audio::engine_start_sequence);
            break;
          case flight::engine_key_event::start_synth: lapc_effect(audio::lapc_effect_cue::engine_start);break;
          case flight::engine_key_event::stop_synth: lapc_effect(audio::lapc_effect_cue::engine_stop);break;
          }
          if(request.running) {
            (side == flight::engine_side::left ? player.left_engine : player.right_engine) = *request.running;
            auto const condition{*request.running ? flight::system_condition::active : flight::system_condition::normal};
            player_damage.systems[std::to_underlying(system)] = condition;
            physical_player.powerplant.engine_condition[index] = condition;
            physical_player.powerplant.engine_factor[index] = *request.running ? 1 : 0;
          }
        }};
        if(pressed(GLFW_KEY_LEFT_BRACKET)) engine_key(flight::engine_side::left);
        if(pressed(GLFW_KEY_RIGHT_BRACKET)) engine_key(flight::engine_side::right);
        if(automatic_landing.phase == tfx::flight::automatic_landing_phase::inactive && !physical_player.manual.controls.autopilot.enabled) {
          auto change{tfx::flight::afterburner_change::none};
          if(pressed(GLFW_KEY_KP_MULTIPLY)) change = down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT) ? physical_player.powerplant.decrease_afterburner() : physical_player.powerplant.increase_afterburner(physical_player.aircraft.powerplant.afterburner_stages);
          if(pressed(GLFW_KEY_MINUS) || pressed(GLFW_KEY_KP_SUBTRACT)) change = physical_player.powerplant.decrease_afterburner();
          if(pressed(GLFW_KEY_SLASH) && physical_player.powerplant.stop_afterburner()) change = tfx::flight::afterburner_change::stopped;
          player.afterburner = physical_player.powerplant.afterburner;
          player.throttle = physical_player.powerplant.throttle;
          reheat_sound(change);
          if(change == flight::afterburner_change::started || change == flight::afterburner_change::increased) lapc_effect(audio::lapc_effect_cue::afterburner_start);
          if(change == tfx::flight::afterburner_change::started) scene.actor_event(player_visual,9);
          else if(change == tfx::flight::afterburner_change::stopped) scene.actor_event(player_visual,10);
        }
        if(pressed(GLFW_KEY_H) && !down(GLFW_KEY_LEFT_ALT) && !down(GLFW_KEY_RIGHT_ALT)) {
          player.arrestor_hook = !player.arrestor_hook;
          if(!player.arrestor_hook) player.arrested = false;
        }
        if(pressed(GLFW_KEY_W) && player.landed) player.wheel_brake = !player.wheel_brake;
        if(pressed(GLFW_KEY_B) && !player.collision && !player_disabled && automatic_landing.phase == tfx::flight::automatic_landing_phase::inactive) {
          if(down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT)) {
            if(player.aircraft != tfx::flight::player_aircraft::raptor && !player.brake_chute_cut_away && player.fixed_speed >= 20) { player.brake_chute = true;scene.actor_event(player_visual,15); }
          } else if(down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) {
            if(player.aircraft != tfx::flight::player_aircraft::raptor && player.brake_chute) { player.brake_chute = false;player.brake_chute_cut_away = true;scene.actor_event(player_visual,16); }
          } else {
            auto &condition{player_damage.systems[std::to_underlying(tfx::flight::aircraft_system::air_brake)]};
            if(physical_player.aircraft.surfaces.air_brake_drag != 0 && condition != tfx::flight::system_condition::failed) {
              // Native 84F68 toggles the retained fraction, then only the
              // normal/active switch states. Fault conditions remain faults.
              physical_player.manual.air_brake_fraction = static_cast<double>(1.0L - physical_player.manual.air_brake_fraction);
              if(condition == tfx::flight::system_condition::normal) condition = tfx::flight::system_condition::active;
              else if(condition == tfx::flight::system_condition::active) condition = tfx::flight::system_condition::normal;
              player.air_brake = condition == tfx::flight::system_condition::active;
            }
          }
          sample(audio::sample_priority::normal,audio::sample_id::air_brake);
          lapc_effect(audio::lapc_effect_cue::mechanical_switch);
        }
      }
      timer_fraction += real_elapsed * tfx::simulation::timer_hz;
      auto const ticks{static_cast<tfx::simulation::timer_counter>(std::floor(timer_fraction))};
      timer_fraction -= ticks;
      counter += ticks;
      bool const view_modifier{down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT) || down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)};
      bool const steering{!view_modifier && (down(GLFW_KEY_UP) || down(GLFW_KEY_DOWN) || down(GLFW_KEY_LEFT) || down(GLFW_KEY_RIGHT))};
      acceleration.resolve(steering,physical_player.manual.controls.autopilot.enabled && physical_player.manual.controls.autopilot.mode == 4);
      auto const step{flight_clock.poll(counter,{
        .model{preferences.model},
        .time_warp{acceleration.active},
        .time_warp_allowed{!player.landed},
      })};
      if(step) {
        auto const stick_action{simple_controls.source == tfx::flight::control_source::joystick ? stick_buttons.advance(stick_mask,preferences.stick_type,step->world_ticks) : tfx::flight::joystick_action::none};
        switch(stick_action) {
          case tfx::flight::joystick_action::fire: store_trigger = true;break;
          case tfx::flight::joystick_action::ground_weapon: select_weapon(tfx::weapons::weapon_role::ground);break;
          case tfx::flight::joystick_action::air_weapon: select_weapon(tfx::weapons::weapon_role::air);break;
          case tfx::flight::joystick_action::cycle_target: cycle_target();break;
          case tfx::flight::joystick_action::front_view: select_view(tfx::cockpit::view_key::front);break;
          case tfx::flight::joystick_action::left_view: select_view(tfx::cockpit::view_key::left);break;
          case tfx::flight::joystick_action::right_view: select_view(tfx::cockpit::view_key::right);break;
          case tfx::flight::joystick_action::rear_view: select_view(tfx::cockpit::view_key::rear);break;
          case tfx::flight::joystick_action::none: break;
        }
        // Native 0x86820 updates HUD intensity once per flight input frame.
        // Either held key requests a palette upload, even if both cancel out.
        bool const hud_up{down(GLFW_KEY_Y)}, hud_down{down(GLFW_KEY_U)};
        if(hud_up || hud_down) palette_selection.invalidate_normal();
        int constexpr minimum_hud_green{90}, maximum_hud_green{127};
        hud_green = static_cast<int16_t>(std::clamp(hud_green + (hud_up ? 1 : 0) - (hud_down ? 1 : 0),minimum_hud_green,maximum_hud_green));
        // Arcade consumes relative mouse motion; the other original models
        // consume these independently sampled, retained keyboard flags.
        simple_controls.keys = keyboard.directions;
        simple_controls.alt = down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT);
        auto const input_word{[](double const value){ return static_cast<int16_t>(std::clamp(value,-32768.0,32767.0)); }};
        tfx::flight::flight_controls const controls{
          .pitch_delta{designator_view ? int16_t{0} : input_word(pitch_input)},
          .roll_delta{designator_view ? int16_t{0} : input_word(roll_input)},
          .accelerate{keyboard.accelerate},
          .decelerate{keyboard.decelerate},
          .mouse_buttons{static_cast<uint8_t>((focused && !designator_view && glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ? 1 : 0) | (focused && !designator_view && glfwGetMouseButton(host,GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ? 2 : 0))},
        };
        pitch_input -= controls.pitch_delta;
        roll_input -= controls.roll_delta;
        if(!designator_view && simple_controls.source == tfx::flight::control_source::mouse && !arcade_mode) mouse_control.advance_clamped({controls.roll_delta,controls.pitch_delta});
        advance_world(controls,step->world_ticks,step->physics_seconds,down(GLFW_KEY_SPACE),store_trigger);
        if(service_modal_closed) {
          service_modal_closed = false;
          previous_time = glfwGetTime();
          timer_fraction = 0;
          captured = mouse_started = false;
          held.fill(false);
          pitch_input = roll_input = 0;
        }
        store_trigger = false;
        frame_ticks = step->world_ticks;
        flight_clock.begin({
          .model{preferences.model},
        });
      } else {
        glfwWaitEventsTimeout(.002);
        continue;
      }
    }
    if(paused || result_evaluated) glfwWaitEventsTimeout(.01);
    draw();
    window.present(display_frame.colourise(*frame_colours),display_frame.size);
    if(frame_ticks) {
      finish_world(*frame_ticks);
      projectiles.finish_frame();
      std::erase_if(trail_samplers,[&](auto const &entry) { return !projectiles.projectiles().contains(entry.first); });
    }
    if(frontend_flight && result_evaluated) break;
    ++frames;
    ++interval_frames;
    if(now - title_time >= .5) {
      window.title(std::string{inspect ? "TFX Redux — scene inspection — " : "TFX Redux — " + std::string{simple_mode ? "Simple" : arcade_mode ? "Arcade" : preferences.model == simulation::flight_model::realistic ? "Realistic" : "Military-spec"} + " flight — "} + mission_name + " / " + environment.name + " / " + environment.lighting_name + " — " + std::to_string(static_cast<int>(interval_frames / (now - title_time))) + " fps");
      title_time = now;
      interval_frames = 0;
    }
    if(frame_limit && frames >= frame_limit) break;
  }
  if(result_evaluated && !inspect && !frame_limit && flight_recorder.frame_count() && !glfwWindowShouldClose(host)) {
    try {
      flight_recorder.finish();
      if(sound) sound->end_flight();
      auto const camera_y{scene.last_camera() ? tfx::math::signed_word(static_cast<uint16_t>(scene.last_camera()->world_anchor.y)) : int16_t{0}};
      options.projection_origin = {159,99};
      replay_recording(window,game,tables,scene,renderer,options.data_directory / options.recording_name,camera_y,ground_phase.sample(),infrared_dac);
    } catch(std::exception const &error) { std::cerr << "Cannot replay flight recording: " << error.what() << '\n'; }
  }
  glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
  return result_evaluated ? flight_result::finished : flight_result::quit;
}

} // namespace tfx::platform
