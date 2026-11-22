#include <iostream>
#include <string_view>
#include <utility>
#include "cockpit/view_camera.h"

int main(int const argc, char const *const argv[]) {
  bool refuelling{false};
  bool split_basis{false};
  bool horizon{false};
  bool projection{false};
  bool model_submission{false};
  for(int index{1}; index != argc; ++index) {
    auto const argument{std::string_view{argv[index]}};
    if(argument == "--refuelling") {
      refuelling = true;
    } else if(argument == "--projection") {
      projection = true;
    } else if(argument == "--horizon") {
      horizon = true;
    } else if(argument == "--model-submission") {
      model_submission = true;
    } else if(argument == "--split-basis") {
      split_basis = true;
    } else {
      return 2;
    }
  }
  using tfx::cockpit::missile_camera_pose;
  using tfx::cockpit::view_camera;
  using tfx::cockpit::view_camera_context;
  using tfx::cockpit::view_control;
  using tfx::cockpit::view_control_state;
  using tfx::cockpit::view_mode;
  view_control views;
  view_camera motion;
  bool reset;
  int mode;
  while(std::cin >> reset >> mode) {
    view_camera_context context;
    std::cin >> context.position.x >> context.position.y >> context.position.z >> context.position_q8.x >> context.position_q8.y >> context.position_q8.z
      >> context.angles.x >> context.angles.y >> context.angles.z >> context.elapsed >> context.pan >> context.ground_height >> context.low_memory;
    for(unsigned int row{0}; row != 3; ++row) std::cin >> context.basis.right[row] >> context.basis.down[row] >> context.basis.forward[row];
    bool tracked;
    bool observed;
    vec3<int32_t> target;
    vec3<int32_t> observer;
    std::cin >> tracked >> target.x >> target.y >> target.z >> observed >> observer.x >> observer.y >> observer.z >> context.speed >> context.landing;
    context.position_f32 = vec3f{context.position_q8} / 256.0f;
    if(tracked) context.tracking_target_q8.emplace(vec3<int32_t>{target});
    if(observed) {
      context.observed_target_q8.emplace(vec3<int32_t>{observer});
    } else if(tracked) context.observed_target_q8.emplace(vec3<int32_t>{target});
    bool missile_available;
    bool impact_available;
    missile_camera_pose missile;
    vec3<int32_t> impact;
    std::cin >> missile_available >> missile.position_q8.x >> missile.position_q8.y >> missile.position_q8.z >> missile.angles.x >> missile.angles.y >> missile.angles.z >> impact_available >> impact.x >> impact.y >> impact.z;
    if(missile_available) context.missile.emplace(std::move(missile));
    if(impact_available) context.impact_target_q8.emplace(vec3<int32_t>{impact});
    if(reset) {
      view_control_state initial;
      initial.current = static_cast<view_mode>(mode);
      std::cin >> initial.distance >> initial.recenter >> initial.departure_delay >> initial.tracking_roll >> initial.target_distance >> initial.fixed_position.x >> initial.fixed_position.y >> initial.fixed_position.z;
      for(unsigned int row{0}; row != 3; ++row) std::cin >> initial.orbit.right[row] >> initial.orbit.down[row] >> initial.orbit.forward[row];
      views = view_control{std::move(initial)};
      motion = view_camera{};
      motion.initialise_ground_reference(context.position);
    }
    tfx::math::attitude body;
    if(split_basis) {
      for(unsigned int row{0}; row != 3; ++row) std::cin >> body.right[row] >> body.down[row] >> body.forward[row];
      context.body_basis = &body;
    }
    if(refuelling) {
      unsigned int phase;
      std::cin >> phase;
      context.refuel_phase = static_cast<uint8_t>(phase);
    }
    auto active{views.value()};
    active.ready = false;
    views = view_control{std::move(active)};
    bool const handled{motion.advance(views, context)};
    if(!handled) return 2;
    auto const &state{motion.value()};
    auto const &control{views.value()};
    auto const &pose{motion.last_pose()};
    auto const put{[](auto const value){std::cout << static_cast<int32_t>(value) << ' ';}};
    put(control.distance);
    put(control.recenter);
    put(control.departure_delay);
    put(control.ready);
    for(unsigned int row{0}; row != 3; ++row) {
      put(control.orbit.right[row]);
      put(control.orbit.down[row]);
      put(control.orbit.forward[row]);
    }
    put(state.head.x);
    put(state.head.y);
    put(state.head.z);
    put(state.offset_q16.x);
    put(state.offset_q16.y);
    put(state.offset_q16.z);
    put(state.smoothed_angles.x);
    put(state.smoothed_angles.y);
    put(state.smoothed_angles.z);
    put(state.pitch);
    put(state.heading);
    put(state.orbit_roll);
    put(state.previous_floor_offset);
    put(pose.anchor.x);
    put(pose.anchor.y);
    put(pose.anchor.z);
    put(pose.position_q16.x);
    put(pose.position_q16.y);
    put(pose.position_q16.z);
    for(auto const value : pose.matrix.data) put(value);
    put(pose.player_model_entry ? static_cast<int>(*pose.player_model_entry) : -1);
    put(control.current);
    put(control.target_distance);
    put(control.written);
    put(control.read);
    for(auto const mode : control.queue) put(mode);
    put(control.tracking_complete);
    put(control.tracking_roll);
    put(state.pan);
    put(state.last_missile.position_q8.x);
    put(state.last_missile.position_q8.y);
    put(state.last_missile.position_q8.z);
    put(state.last_missile.angles.x);
    put(state.last_missile.angles.y);
    put(state.last_missile.angles.z);
    put(control.fixed_position.x);
    put(control.fixed_position.y);
    put(control.fixed_position.z);
    if(horizon) {
      put(pose.horizon_angles.has_value());
      put(pose.horizon_angles ? pose.horizon_angles->x : 0);
      put(pose.horizon_angles ? pose.horizon_angles->y : 0);
      put(pose.horizon_angles ? pose.horizon_camera_y : 0);
      put(pose.ground_motion.has_value());
      put(pose.ground_motion.value_or(0));
      put(state.ground_reference.x);
      put(state.ground_reference.y);
      put(state.ground_reference.z);
    }
    if(model_submission) put(pose.player_model_entry ? std::to_underlying(pose.model_source) : -1);
    if(projection) put(tfx::render::projection_shift(pose.opening));
    std::cout << '\n';
  }
}
