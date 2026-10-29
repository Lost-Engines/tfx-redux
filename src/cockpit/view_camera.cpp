#include "view_camera.h"
#include "render/continuous_pose.h"
#include <algorithm>
#include <bit>
#include <utility>

namespace tfx::cockpit {
namespace {
int32_t wrap(uint32_t const value) { return std::bit_cast<int32_t>(value); }
int32_t add(int32_t const a, int32_t const b) { return wrap(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t subtract(int32_t const a, int32_t const b) { return wrap(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int16_t word(int const value) { return math::signed_word(static_cast<uint16_t>(value)); }
math::angle negative(math::angle const value) { return static_cast<math::angle>(-value); }
math::attitude product(math::attitude const &a, math::attitude const &b) {
  auto const transform{[&](vec3<int32_t> const &column) {
    auto const sum{vec3<int64_t>{a.right} * column.x + vec3<int64_t>{a.down} * column.y + vec3<int64_t>{a.forward} * column.z};
    return vec3<int32_t>{word(wrap(static_cast<uint32_t>(sum.x)) >> 15),word(wrap(static_cast<uint32_t>(sum.y)) >> 15),word(wrap(static_cast<uint32_t>(sum.z)) >> 15)};
  }};
  return {
    .right{transform(b.right)},
    .down{transform(b.down)},
    .forward{transform(b.forward)},
  };
}
}

render::camera view_pose::camera(vec2d const &projection_origin) const {
  auto result{render::original_camera(position_q16,matrix,projection_origin,opening)};
  result.view_angles = vec3<math::angle>{angles};
  result.world_anchor = vec3<int32_t>{anchor};
  // The native renderer wraps each world coordinate to a word. Keep its Q16
  // translation but unwrap it around the full terrain anchor for the scene host.
  for(unsigned int axis{0}; axis != 3; ++axis) result.position[axis] = anchor[axis] + subtract(position_q16[axis],wrap(static_cast<uint32_t>(anchor[axis]) << 16)) / 65536.0;
  return result;
}
view_camera::view_camera(view_motion_state initial) : state{std::move(initial)}, display_angles{render::angle_radians(state.smoothed_angles)}, display_basis{render::continuous_rotation(display_angles)}, display_offset{vec3d{state.offset_q16}/65536} {}
view_motion_state const &view_camera::value() const { return state; }
view_pose const &view_camera::last_pose() const { return pose; }
render::camera view_camera::display_camera(vec2d const &projection_origin) const {
  auto result{render::continuous_camera(pose.camera(projection_origin),display_basis)};
  if(display_position)result.position=vec3d{*display_position};
  return result;
}
void view_camera::set_angles(vec3<math::angle> const &angles) {
  display_position.reset();
  pose.angles = vec3<math::angle>{angles};
  pose.horizon_angles.emplace(angles.x,angles.z);
  pose.horizon_camera_y = math::signed_word(static_cast<uint16_t>(pose.position_q16.y >> 16));
  pose.matrix = math::view_rotation_words(angles);
  display_angles=-render::angle_radians(angles);
  display_basis=render::continuous_rotation(display_angles);
}
void view_camera::player_origin(view_camera_context const &context) {
  pose.anchor = vec3<int32_t>{context.position};
  for(unsigned int axis{0}; axis != 3; ++axis) pose.position_q16[axis] = wrap(static_cast<uint32_t>(context.position_q8[axis]) << 8);
}
void view_camera::pan_head(view_control &views, uint16_t const pan, bool const limit_pitch) {
  auto transposed{views.state.orbit.transposed()};
  int const pitch{math::signed_word(views.state.orbit.angles().x)};
  if(pan & 1) transposed.rotate(math::rotation_axis::heading,0x400);
  if(pan & 2) transposed.rotate(math::rotation_axis::heading,0xfc00);
  views.state.orbit = transposed.transposed();
  if((pan & 4) && (!limit_pitch || pitch <= 0)) views.state.orbit.rotate(math::rotation_axis::pitch,0x400);
  if(pan & 8) views.state.orbit.rotate(math::rotation_axis::pitch,0xfc00);
}
void view_camera::limit_head(view_control &views) {
  auto transposed{views.state.orbit.transposed()};
  auto const angles{transposed.angles()};
  state.pitch = math::signed_word(angles.x);
  state.heading = math::signed_word(angles.y);
  int const index{state.head.z < 0 ? -state.head.z : state.head.z};
  // The native three-position head lift allows wider yaw/pitch away from the panel.
  static std::array<vec2<int16_t>,3> constexpr limits{{{6656,17664},{5632,12288},{768,4864}}};
  auto const &limit{limits.at(static_cast<unsigned int>(index))};
  if(state.heading > limit.y) transposed.rotate(math::rotation_axis::heading,0xfe00);
  if(state.heading < -limit.y) transposed.rotate(math::rotation_axis::heading,0x200);
  views.state.orbit = transposed.transposed();
  if(state.pitch > limit.x) views.state.orbit.rotate(math::rotation_axis::pitch,0x200);
  if(state.pitch < -limit.x) views.state.orbit.rotate(math::rotation_axis::pitch,0xfe00);
}
void view_camera::select_head_height() {
  int const heading{state.heading};
  if(heading >= 0) {
    if(heading < 0x2000) state.head.z = -2;
    else if(heading < 0x3000) state.head.z = -1;
    else if(heading < 0x4000) state.head.z = 0;
    else if(heading < 0x5000) state.head.z = 1;
  } else {
    if(heading > -0x2000) state.head.z = -2;
    else if(heading > -0x3000) state.head.z = -1;
    else if(heading > -0x4000) state.head.z = 0;
    else if(heading > -0x5000) state.head.z = 1;
  }
}
bool view_camera::compose(view_control &views, view_camera_context const &context, bool const chase) {
  auto const &world{context.basis};
  auto const precise_world{render::continuous_basis(world)};
  double const roll{chase ? render::angle_radians(static_cast<math::angle>(state.orbit_roll)) : -render::continuous_angles(precise_world).z/16};
  auto const target{precise_world*render::continuous_basis(views.state.orbit)*render::continuous_rotation(vec3d{0,0,roll})};
  auto const following{render::follow_angles(display_angles,render::continuous_angles(target),chase ? .125 : .25,!chase && views.state.tracking_roll)};
  auto const head_offset{precise_world*vec3d{state.head}/8};
  auto const setback{target*vec3d{0,0,-views.state.distance/2.0}};
  if(views.state.recenter)display_offset=chase ? setback-head_offset : vec3d{0,0,0};
  else display_offset=display_offset.lerp(chase ? .125 : .25,setback+head_offset);
  auto combined{product(world,views.state.orbit)};
  if(chase) combined.rotate(math::rotation_axis::roll,static_cast<math::angle>(state.orbit_roll));
  else combined.rotate(math::rotation_axis::roll,static_cast<math::angle>((-math::signed_word(context.angles.z)) >> 4));
  vec3<int32_t> head;
  for(unsigned int row{0}; row != 3; ++row) head[row] = add(add(world.right[row] * state.head.x,world.down[row] * state.head.y),world.forward[row] * state.head.z) / 4;
  int const distance{-views.state.distance};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    int32_t const target{combined.forward[axis] * distance};
    if(views.state.recenter) state.offset_q16[axis] = chase ? subtract(target,head[axis]) : 0;
    else {
      auto const delta{add(subtract(target,state.offset_q16[axis]),head[axis])};
      state.offset_q16[axis] = add(state.offset_q16[axis],chase ? delta / 8 : delta >> 2);
    }
  }
  if(views.state.recenter) views.state.recenter = chase ? 0 : word(views.state.recenter - 1);
  auto const angles{combined.angles()};
  for(unsigned int axis{0}; axis != 3; ++axis) {
    if(!chase && axis == 2) {
      if(views.state.tracking_roll) state.smoothed_angles.z = angles.z;
      else state.smoothed_angles.z = static_cast<math::angle>(state.smoothed_angles.z - (word(state.smoothed_angles.z - angles.z) >> 2));
    } else {
      int const delta{word(angles[axis] - state.smoothed_angles[axis])};
      state.smoothed_angles[axis] = static_cast<math::angle>(state.smoothed_angles[axis] + (delta >> (chase ? 3 : 2)));
    }
  }
  player_origin(context);
  for(unsigned int axis{0}; axis != 3; ++axis) pose.position_q16[axis] = add(pose.position_q16[axis],state.offset_q16[axis]);
  bool clamped{false};
  if(chase) {
    int32_t const floor{wrap(static_cast<uint32_t>(context.ground_height - 5) << 16)};
    if(pose.position_q16.y > floor) {
      clamped = true;
      pose.position_q16.y = floor;
      state.previous_floor_offset = state.offset_q16.y;
    }
  }
  set_angles({negative(state.smoothed_angles.x),negative(state.smoothed_angles.y),negative(state.smoothed_angles.z)});
  // Preserve the native pose/state above for gameplay and reference checks;
  // the visible camera keeps sub-phase angles through the same follow filter.
  display_angles=vec3d{following};
  display_basis=render::continuous_rotation(display_angles);
  display_position=vec3d{context.position_q8}/256+display_offset;
  if(chase)display_position->y=std::min(display_position->y,context.ground_height-5.0);
  // Native 0x9095c chooses the internal canopy/side model only while the
  // eye remains within 0xa000 Q16 units of the player anchor.
  bool near{!chase};
  for(auto const offset : {state.offset_q16.x,state.offset_q16.y,state.offset_q16.z}) near = near && offset > -0xa000 && offset < 0xa000;
  pose.player_model_entry = (near ? (state.heading > -0x2000 && state.heading < 0x2000 ? 19u : 20u) : 21u) + context.refuel_phase;
  return clamped;
}
void view_camera::initialise_ground_reference(vec3<int32_t> const &position) { state.ground_reference = vec3<int32_t>{position}; }
void view_camera::advance_ground_reference(view_camera_context const &context, bool const inhibited) {
  pose.ground_motion.reset();
  if(inhibited) return; // Native 0x92969 excludes view 4 and retains its reference.
  int32_t const x{word(static_cast<uint32_t>(state.ground_reference.x) - static_cast<uint32_t>(context.position.x))};
  int32_t const z{word(static_cast<uint32_t>(state.ground_reference.z) - static_cast<uint32_t>(context.position.z))};
  auto const transformed{[&](vec3<int32_t> const &axis) {
    uint32_t const product{static_cast<uint32_t>(word(axis.x)) * static_cast<uint32_t>(x) + static_cast<uint32_t>(word(axis.z)) * static_cast<uint32_t>(z)};
    return math::signed_word(static_cast<uint16_t>(wrap(product) >> math::matrix_fraction_bits));
  }};
  pose.ground_motion = math::distance_word({transformed(context.basis.right),transformed(context.basis.down),transformed(context.basis.forward)});
  state.ground_reference = vec3<int32_t>{context.position};
}

bool view_camera::advance(view_control &views, view_camera_context const &context) {
  auto &control{views.state};
  pose.opening = render::aperture::normal;
  int16_t const previous_projection_y{math::signed_word(static_cast<uint16_t>(pose.position_q16.y >> 16))};
  pose.horizon_angles.reset();
  pose.player_model_entry.reset();
  pose.model_source = player_model_source::fixed;
  pose.ground_motion.reset();
  state.pan = context.pan;
  auto const tracking_angles{[&] {
    if(!context.tracking_target_q8) return vec2<math::angle>{0,0};
    vec3<int32_t> const origin{wrap(static_cast<uint32_t>(context.position.x) << 8),wrap(static_cast<uint32_t>(context.position.y) << 8),wrap(static_cast<uint32_t>(context.position.z) << 8)};
    auto const bearing{math::bearing(origin,*context.tracking_target_q8)};
    return vec2<math::angle>{static_cast<math::angle>(-bearing.x - 1 - 0x800),static_cast<math::angle>(-bearing.y - 1)};
  }};
  auto const resume_tracking{[&] {
    int const roll{math::signed_word(context.angles.z)};
    if(control.tracking_complete == 1 && context.tracking_target_q8 && roll < 0x2a00 && roll >= -0x2a00) views.enqueue(view_mode::target_tracking,context.speed);
  }};
  auto const head_pose{[&] { compose(views,context,false); }};
  auto const transpose{[&] { return control.orbit.transposed(); }};
  switch(control.current) {
  case view_mode::cockpit:
    player_origin(context);
    set_angles({negative(context.angles.x),negative(context.angles.y),negative(context.angles.z)});
    display_basis=render::continuous_basis(context.basis);
    display_angles=render::continuous_angles(display_basis);
    views.ready();
    break;
  case view_mode::full_forward: {
    player_origin(context);
    set_angles({negative(context.angles.x),negative(context.angles.y),negative(context.angles.z)});
    // Native 0xb96dc uses the current basis directly, preserving its stored precision.
    auto const &basis{context.basis};
    pose.matrix.data = {basis.right.x,basis.right.y,basis.right.z,basis.down.x,basis.down.y,basis.down.z,basis.forward.x,basis.forward.y,basis.forward.z};
    display_basis=render::continuous_basis(basis);
    display_angles=render::continuous_angles(display_basis);
    views.ready();
    break;
  }
  case view_mode::rear:
    control.orbit = math::attitude{};
    control.orbit.rotate(math::rotation_axis::heading,0x8000);
    control.distance = 0;
    state.head.z = 0;
    control.recenter = 1;
    state.heading = math::signed_word(control.orbit.angles().y);
    head_pose();
    views.ready();
    break;
  case view_mode::leave_cockpit:
    control.orbit = math::attitude{};
    control.distance = 0;
    state.head.z = 0;
    pan_head(views,context.pan,true);
    limit_head(views);
    head_pose();
    views.ready();
    break;
  case view_mode::virtual_cockpit:
    control.distance = 0;
    state.head.z = -2;
    pan_head(views,context.pan,true);
    limit_head(views);
    head_pose();
    views.ready();
    resume_tracking();
    break;
  case view_mode::look_left:case view_mode::look_right:
  case view_mode::look_far_left:case view_mode::look_far_right: {
    bool const left{control.current == view_mode::look_left || control.current == view_mode::look_far_left};
    bool const far{control.current == view_mode::look_far_left || control.current == view_mode::look_far_right};
    auto basis{transpose()};
    state.heading = math::signed_word(basis.angles().y);
    select_head_height();
    basis.rotate(math::rotation_axis::heading,left ? 0x400 : 0xfc00);
    int const limit{far ? 0x6800 : 0x1800};
    if(left ? state.heading > limit : state.heading < -limit) {
      views.ready();
      basis.rotate(math::rotation_axis::heading,left ? 0xfc00 : 0x400);
    }
    if(far) {
      auto retained{basis};
      basis.rotate(math::rotation_axis::pitch,0xf800);
      control.orbit = basis.transposed();
      head_pose();
      control.orbit = retained.transposed();
    } else {
      control.orbit = basis.transposed();
      head_pose();
    }
    break;
  }
  case view_mode::centre_head: {
    auto basis{transpose()};
    auto const angles{basis.angles()};
    auto const previous{state.heading};
    state.pitch = math::signed_word(angles.x);
    state.heading = word(-angles.y);
    state.head.z = -2;
    if(previous == state.heading) {
      basis.rotate(math::rotation_axis::heading,static_cast<math::angle>(previous));
      state.heading = 0;
    } else if(state.heading) basis.rotate(math::rotation_axis::heading,static_cast<math::angle>(state.heading >> 2));
    control.orbit = basis.transposed();
    if(state.heading > -160 && state.heading < 160) views.ready();
    head_pose();
    resume_tracking();
    break;
  }
  case view_mode::leave_panel:
    control.distance = 0;
    if(state.head.z > -2) state.head.z = word(state.head.z - 1);
    else { state.head.z = -2;views.ready(); }
    limit_head(views);
    head_pose();
    break;
  case view_mode::enter_cockpit:
    if(state.head.z < 0) state.head.z = word(state.head.z + 1);
    else { state.head.z = 0;views.ready(); }
    limit_head(views);
    head_pose();
    break;
  case view_mode::approach_cockpit: {
    int const previous{control.distance};
    control.distance = word(previous - (previous > 5 ? 5 : 1));
    if(previous <= 0) { control.distance = 0;state.head.z = -2;views.ready(); }
    auto basis{transpose()};
    auto const angles{basis.angles()};
    state.pitch = math::signed_word(angles.x);
    state.heading = word(-angles.y);
    if(state.heading) basis.rotate(math::rotation_axis::heading,static_cast<math::angle>(state.heading / 8));
    control.orbit = basis.transposed();
    if(state.pitch) control.orbit.rotate(math::rotation_axis::pitch,static_cast<math::angle>(state.pitch / 8));
    head_pose();
    break;
  }
  case view_mode::depart_cockpit:
    control.orbit = math::attitude{};
    if(control.departure_delay > 0) {
      control.orbit.rotate(math::rotation_axis::pitch,0x2000);
      control.distance = 5;
      control.departure_delay = word(control.departure_delay - context.elapsed);
    } else {
      if(control.distance < 40) control.distance = word(control.distance + (static_cast<unsigned int>(context.elapsed) * 16) / 30);
      else views.ready();
      control.orbit.rotate(math::rotation_axis::heading,0xfa00);
      control.orbit.rotate(math::rotation_axis::pitch,0x800);
    }
    head_pose();
    break;
  case view_mode::fly_past: {
    auto const precise{render::continuous_bearing(vec3d{control.fixed_position},vec3d{context.position_f32})};
    auto following{render::follow_angles(display_angles,precise,.5,false)};
    following.z=display_angles.z*.75;
    auto const bearing{math::bearing(control.fixed_position,context.position)};
    for(unsigned int axis{0}; axis != 2; ++axis) state.smoothed_angles[axis] = static_cast<math::angle>(state.smoothed_angles[axis] + (word(bearing[axis] - state.smoothed_angles[axis]) >> 1));
    state.smoothed_angles.z = static_cast<math::angle>(state.smoothed_angles.z + ((-math::signed_word(state.smoothed_angles.z)) >> 2));
    set_angles({negative(state.smoothed_angles.x),negative(state.smoothed_angles.y),negative(state.smoothed_angles.z)});
    display_angles=vec3d{following};display_basis=render::continuous_rotation(display_angles);
    pose.anchor = vec3<int32_t>{control.fixed_position};
    for(unsigned int axis{0}; axis != 3; ++axis) pose.position_q16[axis] = wrap(std::rotl(static_cast<uint32_t>(control.fixed_position[axis]),16));
    pose.player_model_entry = 21u + context.refuel_phase;
    int const relative{word(bearing.y - context.angles.y)};
    if(relative && relative > -0x4000 && relative < 0x4000 && math::distance_estimate(context.position,control.fixed_position) > context.speed / 3) {
      views.clear_queue();
      views.enqueue(view_mode::chase,context.speed);
      views.ready();
      for(unsigned int axis{0}; axis != 3; ++axis) state.offset_q16[axis] = wrap(static_cast<uint32_t>(subtract(control.fixed_position[axis],context.position[axis])) << 16);
      display_offset=vec3d{control.fixed_position}-vec3d{context.position_q8}/256;
      control.orbit = math::attitude{};
      control.distance = 40;
    }
    if(context.landing) views.ready();
    break;
  }
  case view_mode::missile: {
    views.ready();
    if(context.missile) {
      state.last_missile = *context.missile;
      control.distance = 80;
    } else {
      if(context.impact_target_q8) views.enqueue(view_mode::impact_orbit,context.speed);
      control.distance = word(control.distance + context.elapsed * 16);
      if(control.distance > 1000) {
        views.enqueue(view_mode::tactical,context.speed);
        control.recenter = 1;
        control.distance = 40;
      }
    }
    math::attitude missile;
    missile.rotate(math::rotation_axis::heading,state.last_missile.angles.y);
    missile.rotate(math::rotation_axis::pitch,state.last_missile.angles.x);
    missile.rotate(math::rotation_axis::roll,state.last_missile.angles.z);
    auto orbit{control.orbit.transposed()};
    if(context.pan & 1) orbit.rotate(math::rotation_axis::heading,0x200);
    if(context.pan & 2) orbit.rotate(math::rotation_axis::heading,0xfe00);
    control.orbit = orbit.transposed();
    if(context.pan & 4) control.orbit.rotate(math::rotation_axis::pitch,0x200);
    if(context.pan & 8) control.orbit.rotate(math::rotation_axis::pitch,0xfe00);
    auto combined{product(missile,control.orbit)};
    if(context.pan & 0x40) state.orbit_roll = word(state.orbit_roll + 0x200);
    if(context.pan & 0x80) state.orbit_roll = word(state.orbit_roll - 0x200);
    combined.rotate(math::rotation_axis::roll,static_cast<math::angle>(state.orbit_roll));
    if(context.pan & 0x10) control.distance = word(control.distance - 5);
    if(context.pan & 0x20) control.distance = word(control.distance + 5);
    for(unsigned int axis{0}; axis != 3; ++axis) {
      if(control.recenter) state.offset_q16[axis] = 0;
      else {
        int32_t const delta{subtract(combined.forward[axis] * -control.distance,state.offset_q16[axis])};
        state.offset_q16[axis] = add(state.offset_q16[axis],delta / 5);
      }
    }
    bool const recentered{control.recenter!=0};
    control.recenter = 0;
    state.smoothed_angles = combined.angles();
    for(unsigned int axis{0}; axis != 3; ++axis) {
      pose.anchor[axis] = state.last_missile.position_q8[axis] >> 8;
      pose.position_q16[axis] = add(wrap(static_cast<uint32_t>(state.last_missile.position_q8[axis]) << 8),state.offset_q16[axis]);
    }
    set_angles({negative(state.smoothed_angles.x),negative(state.smoothed_angles.y),negative(state.smoothed_angles.z)});
    display_basis=render::continuous_rotation(state.last_missile.angles)*render::continuous_basis(control.orbit)*render::continuous_rotation(vec3d{0,0,render::angle_radians(static_cast<math::angle>(state.orbit_roll))});
    display_angles=render::continuous_angles(display_basis);
    display_offset=recentered ? vec3d{0,0,0} : display_offset.lerp(.2,display_basis*vec3d{0,0,-control.distance/2.0});
    display_position=vec3d{state.last_missile.position_q8}/256+display_offset;
    pose.player_model_entry = 21u + context.refuel_phase;
    pose.model_source = player_model_source::integer;
    break;
  }
  case view_mode::missile_infrared:
    pose.opening = render::aperture::missile_nose;
    views.ready();
    if(context.missile) {
      for(unsigned int axis{0}; axis != 3; ++axis) {
        pose.anchor[axis] = context.missile->position_q8[axis] >> 8;
        pose.position_q16[axis] = wrap(static_cast<uint32_t>(context.missile->position_q8[axis]) << 8);
      }
      set_angles({negative(context.missile->angles.x),negative(context.missile->angles.y),negative(context.missile->angles.z)});
    } else {
      // Native enqueues view 6 then continues through a null missile pointer.
      // Retain the last valid pose for this transition rather than reading DOS's
      // low-memory page as a projectile record in the native host.
      views.enqueue(view_mode::missile,context.speed);
    }
    break;
  case view_mode::impact_orbit: {
    views.ready();
    if(!context.impact_target_q8) {
      views.clear_queue();
      control.fixed_position = vec3<int32_t>{context.position};
      math::advance_position(control.fixed_position,{context.angles.x,static_cast<math::angle>(context.angles.y - 700),context.angles.z},math::signed_word(static_cast<uint16_t>(context.speed >> 1)),context.height_bounds.x,context.height_bounds.y);
      views.enqueue(view_mode::fly_past,context.speed);
      break;
    }
    auto const &target{*context.impact_target_q8};
    control.distance = target.y > -200 ? word(math::signed_word(static_cast<uint16_t>(target.y)) + 500) : 200;
    auto orbit{control.orbit.transposed()};
    orbit.rotate(math::rotation_axis::heading,0x200);
    control.orbit = orbit.transposed();
    // The impact basis is the original identity passed through the same Q15 product.
    auto const combined{product(math::attitude{},control.orbit)};
    for(unsigned int axis{0}; axis != 3; ++axis) {
      int32_t const delta{subtract(combined.forward[axis] * -control.distance,state.offset_q16[axis])};
      state.offset_q16[axis] = add(state.offset_q16[axis],delta / 2);
      pose.anchor[axis] = target[axis] >> 8;
      pose.position_q16[axis] = add(wrap(static_cast<uint32_t>(target[axis]) << 8),state.offset_q16[axis]);
    }
    auto const angles{combined.angles()};
    state.smoothed_angles.x = angles.x;
    state.smoothed_angles.y = angles.y;
    set_angles({negative(angles.x),negative(angles.y),0});
    display_angles=render::continuous_angles(render::continuous_basis(control.orbit));display_angles.z=0;
    display_basis=render::continuous_rotation(display_angles);
    display_offset=display_offset.lerp(.5,render::continuous_basis(control.orbit)*vec3d{0,0,-control.distance/2.0});
    display_position=vec3d{target}/256+display_offset;
    pose.player_model_entry = 21u + context.refuel_phase;
    pose.model_source = player_model_source::integer;
    break;
  }
  case view_mode::target_tracking: {
    auto const target{tracking_angles()};
    int const roll{math::signed_word(context.angles.z)};
    control.tracking_complete = !context.tracking_target_q8 || roll >= 0x3000 || roll <= -0x3000;
    control.tracking_roll = 0;
    auto combined{product(context.basis,control.orbit)};
    combined.rotate(math::rotation_axis::roll,static_cast<math::angle>((-roll) >> 4));
    auto const angles{combined.angles()};
    int const horizontal{word(angles.y + target.y)};
    if(horizontal < -0x400) state.pan |= 2;
    if(horizontal > 0x400) state.pan |= 1;
    if((state.pan & 3) != 3) {
      int const vertical{math::signed_word(angles.x) + math::signed_word(target.x)};
      if(vertical < -0x400 && vertical > -0x4000) state.pan |= 4;
      if(vertical > 0x400 && vertical < 0x4000) state.pan |= 8;
    }
    pan_head(views,state.pan,true);
    state.heading = math::signed_word(control.orbit.angles().y);
    select_head_height();
    head_pose();
    if(control.tracking_complete) {
      control.tracking_roll = 1;
      views.enqueue(view_mode::centre_head,context.speed);
      views.enqueue(view_mode::virtual_cockpit,context.speed);
    }
    views.ready();
    break;
  }
  case view_mode::tactical: {
    auto const angles{tracking_angles()};
    control.distance = 40;
    if(context.pan & 0x10) control.distance = word(control.distance - 1);
    if(context.pan & 0x20) control.distance = word(control.distance + 1);
    player_origin(context);
    auto const offset{math::orbit_offset_q16(angles,control.distance >> 1)};
    for(unsigned int axis{0}; axis != 3; ++axis) pose.position_q16[axis] = add(pose.position_q16[axis],offset[axis]);
    set_angles({angles.x,angles.y,0});
    if(context.tracking_target_q8) {
      display_angles=render::continuous_bearing(vec3d{context.position_q8},vec3d{*context.tracking_target_q8})+vec3d{render::angle_radians(0x801),render::angle_radians(1),0};
      display_basis=render::continuous_rotation(display_angles);
    }
    display_position=vec3d{context.position_q8}/256+display_basis*vec3d{0,0,-static_cast<double>(control.distance>>1)};
    pose.player_model_entry = 21u + context.refuel_phase;
    views.ready();
    pose.horizon_camera_y = previous_projection_y;
    break;
  }
  case view_mode::ejection: {
    // Native 0x91f00 uses the actual float pose and drops the final eight
    // fraction bits after applying its twelve-world-unit camera setback.
    auto basis{context.body_basis ? *context.body_basis : context.basis};
    int32_t constexpr level_near_ground{-80};
    if(context.position.y >= level_near_ground) basis.align_down({0,1,0});
    auto const precise{render::continuous_basis(basis)*render::continuous_rotation(vec3<math::angle>{0,0x6000,0})*render::continuous_rotation(vec3<math::angle>{0x0aaa,0,0})};
    basis.rotate(math::rotation_axis::heading,0x6000);
    basis.rotate(math::rotation_axis::pitch,0x0aaa);
    state.smoothed_angles = basis.angles();
    pose.anchor = vec3<int32_t>{context.position};
    float constexpr normalised_word{3.0518509447574615e-05f}, setback{12}, position_unit{256};
    for(unsigned int axis{0}; axis != 3; ++axis) {
      long double const offset{static_cast<long double>(math::signed_word(static_cast<uint16_t>(basis.forward[axis]))) * normalised_word * setback * position_unit};
      int32_t const coordinate{static_cast<int32_t>(static_cast<long double>(context.position_f32[axis]) * position_unit - offset)};
      pose.position_q16[axis] = wrap(static_cast<uint32_t>(coordinate) << 8);
    }
    set_angles({negative(state.smoothed_angles.x),negative(state.smoothed_angles.y),negative(state.smoothed_angles.z)});
    display_basis=mat3d{precise};display_angles=render::continuous_angles(display_basis);
    display_position=vec3d{context.position_f32}+display_basis*vec3d{0,0,-12};
    pose.player_model_entry = 21u + context.refuel_phase;
    pose.horizon_camera_y = previous_projection_y;
    break;
  }
  case view_mode::unbanked_chase: {
    control.distance = 500;
    player_origin(context);
    auto const offset{math::orbit_offset_q16({negative(context.angles.x),negative(context.angles.y)},60)};
    for(unsigned int axis{0}; axis != 3; ++axis) pose.position_q16[axis] = add(pose.position_q16[axis],offset[axis]);
    set_angles({negative(context.angles.x),negative(context.angles.y),0});
    display_angles=render::continuous_angles(render::continuous_basis(context.basis));display_angles.z=0;
    display_basis=render::continuous_rotation(display_angles);
    display_position=vec3d{context.position_q8}/256+display_basis*vec3d{0,0,-60};
    pose.player_model_entry = 21u + context.refuel_phase;
    pose.horizon_camera_y = previous_projection_y;
    break;
  }
  case view_mode::target: {
    if(!context.observed_target_q8) {
      control.current = control.previous;
      return advance(views,context);
    }
    if(context.pan & 0x10) control.target_distance = word(control.target_distance - context.elapsed * 8);
    if(context.pan & 0x20) control.target_distance = word(control.target_distance + context.elapsed * 8);
    control.target_distance = std::clamp(control.target_distance,int16_t{100},int16_t{2000});
    auto const &target{*context.observed_target_q8};
    // Retain the native variable-shift quirk in its terrain anchor. The actual
    // camera translation below uses the target's full Q8 height independently.
    unsigned int const height_shift{static_cast<unsigned int>(8 - control.target_distance) & 31};
    pose.anchor = {target.x >> 8,(target.y >> height_shift) >> 1,target.z >> 8};
    auto const offset{math::orbit_offset_q16({0xe000,0},control.target_distance >> 1)};
    for(unsigned int axis{0}; axis != 3; ++axis) pose.position_q16[axis] = add(wrap(static_cast<uint32_t>(target[axis]) << 8),offset[axis]);
    set_angles({0xe000,0,0});
    display_position=vec3d{target}/256+display_basis*vec3d{0,0,-static_cast<double>(control.target_distance>>1)};
    pose.player_model_entry = 21u + context.refuel_phase;
    views.ready();
    pose.horizon_camera_y = previous_projection_y;
    break;
  }
  case view_mode::chase: {
    if(context.low_memory && control.distance < 30) control.distance = 40;
    auto const previous{control.orbit};
    pan_head(views,context.pan,false);
    if(context.pan & 0x40) state.orbit_roll = word(state.orbit_roll + 0x200);
    if(context.pan & 0x80) state.orbit_roll = word(state.orbit_roll - 0x200);
    if((context.pan & 0x10) && control.distance > 30) control.distance = word(control.distance - 5);
    if(context.pan & 0x20) control.distance = word(control.distance + 5);
    for(unsigned int axis{0}; axis != 3; ++axis) {
      unsigned int const shift{8 + axis * 2};
      if(context.pan & (2 << shift)) state.head[axis] = word(state.head[axis] - 1);
      if(context.pan & (1 << shift)) state.head[axis] = word(state.head[axis] + 1);
    }
    auto const floor_offset{state.previous_floor_offset};
    bool const clamped{compose(views,context,true)};
    if(clamped && state.offset_q16.y >= floor_offset) control.orbit = previous;
    views.ready();
    break;
  }
  default:
    return false;
  }
  advance_ground_reference(context,control.current == view_mode::chase);
  return true;
}

} // namespace tfx::cockpit
